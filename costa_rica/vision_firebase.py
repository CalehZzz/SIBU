#!/usr/bin/env python3
"""
SIBU — Visión Pi ↔ Firestore (rápido)

Optimizaciones:
  - Cámara USB queda ABIERTA (abrirla en cada foto costaba 1–3 s)
  - Poll de comando 0.4 s (antes 2 s)
  - Una sola captura → JPEG en memoria para preview + Gemini
  - Imagen chica a Gemini (más upload / respuesta)
  - Cliente Gemini reutilizado
"""

from __future__ import annotations

import argparse
import base64
import io
import os
import sys
import time
from pathlib import Path

import firebase_admin
from firebase_admin import credentials, firestore

ROOT = Path(__file__).resolve().parents[1]

MATERIAL_CODE = {
    "plastico": 1,
    "plastic": 1,
    "aluminio": 2,
    "lata": 2,
    "latas": 2,
    "vidrio": 3,
    "glass": 3,
    "desconocido": 4,
    "unknown": 4,
}

PROMPT = (
    "Clasifica el objeto reciclable en la imagen. "
    "Responde UNA sola palabra en minúsculas, sin puntuación: "
    "plastico | aluminio | vidrio | desconocido"
)


class UsbCam:
    """Mantener /dev/video0 abierto entre disparos."""

    def __init__(self, index: int = 0, width: int = 640, height: int = 480):
        import cv2  # type: ignore

        self.cv2 = cv2
        self.index = index
        self.width = width
        self.height = height
        self.cam = None

    def open(self) -> None:
        if self.cam is not None:
            return
        cam = self.cv2.VideoCapture(self.index)
        if not cam.isOpened():
            raise RuntimeError(f"No se abrió la cámara USB index={self.index}")
        # baja resolución = menos tiempo de sensor/USB
        cam.set(self.cv2.CAP_PROP_FRAME_WIDTH, self.width)
        cam.set(self.cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        cam.set(self.cv2.CAP_PROP_BUFFERSIZE, 1)
        # descartar frames viejos del buffer
        for _ in range(4):
            cam.read()
        self.cam = cam
        print(f"Cámara USB abierta ({self.width}x{self.height})")

    def grab_jpeg(self, quality: int = 60) -> bytes:
        self.open()
        assert self.cam is not None
        # 2 lecturas: la 2ª suele ser la fresca con buffer=1
        self.cam.read()
        ok, frame = self.cam.read()
        if not ok or frame is None:
            # reabrir una vez
            self.release()
            self.open()
            self.cam.read()
            ok, frame = self.cam.read()
        if not ok or frame is None:
            raise RuntimeError("No se pudo leer frame de la cámara USB")
        ok2, buf = self.cv2.imencode(
            ".jpg", frame, [int(self.cv2.IMWRITE_JPEG_QUALITY), quality]
        )
        if not ok2:
            raise RuntimeError("imencode JPEG falló")
        return buf.tobytes()

    def release(self) -> None:
        if self.cam is not None:
            self.cam.release()
            self.cam = None


def jpeg_for_gemini(jpeg_bytes: bytes, max_w: int = 512) -> "Image":
    from PIL import Image

    img = Image.open(io.BytesIO(jpeg_bytes)).convert("RGB")
    w, h = img.size
    if w > max_w:
        img = img.resize((max_w, int(h * max_w / w)))
    return img


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--service-account", default=str(ROOT / "serviceAccountKey.json"))
    p.add_argument("--capture-cmd", default=os.environ.get("SIBU_CAPTURE_CMD", ""))
    p.add_argument("--model", default=os.environ.get("GEMINI_MODEL", "gemini-3.8-flash"))
    p.add_argument("--mock-material", default="")
    p.add_argument("--poll", type=float, default=0.4, help="Segundos entre polls de comando")
    p.add_argument("--cam-index", type=int, default=0)
    args = p.parse_args()

    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    mock = args.mock_material.strip().lower()
    if not api_key and not mock:
        print("⚠️  Sin GEMINI_API_KEY (classify fallará; refresh sí funciona)")

    sa = Path(args.service_account)
    if not sa.is_file():
        print(f"❌ No está {sa}")
        sys.exit(1)

    if not firebase_admin._apps:
        firebase_admin.initialize_app(credentials.Certificate(str(sa)))
    fs = firestore.client()
    estado_ref = fs.collection("vision_pi").document("estado")
    cmd_ref = fs.collection("vision_pi").document("comando")

    cam = None if args.capture_cmd else UsbCam(args.cam_index)
    gemini_model = None
    if api_key and not mock:
        print("Cargando cliente Gemini…")
        sys.stdout.flush()
        import google.generativeai as genai

        genai.configure(api_key=api_key)
        gemini_model = genai.GenerativeModel(args.model)
        print("Gemini OK")
        sys.stdout.flush()

    last_nonce = None
    last_hb = 0.0

    def set_estado(**fields):
        estado_ref.set(
            {
                "online": True,
                "pi": True,
                "actualizado": firestore.SERVER_TIMESTAMP,
                **fields,
            },
            merge=True,
        )

    def capture_jpeg() -> bytes:
        t0 = time.monotonic()
        if args.capture_cmd:
            import subprocess
            from pathlib import Path as P

            path = "/tmp/sibu_fb_shot.jpg"
            subprocess.run(args.capture_cmd.format(path=path), shell=True, check=True)
            data = P(path).read_bytes()
        else:
            assert cam is not None
            data = cam.grab_jpeg(quality=60)
        print(f"   captura {len(data)} B en {(time.monotonic()-t0)*1000:.0f} ms")
        return data

    def handle(action: str) -> None:
        t0 = time.monotonic()
        print(f"→ acción: {action}")
        set_estado(busy=True, error=None)
        try:
            jpeg = capture_jpeg()
            b64 = base64.b64encode(jpeg).decode("ascii")
            out = {"jpegBase64": b64, "busy": False, "error": None}
            if action == "classify":
                t1 = time.monotonic()
                if mock:
                    label, code = mock, int(MATERIAL_CODE.get(mock, 4))
                elif gemini_model is None:
                    raise RuntimeError("Falta GEMINI_API_KEY en la Pi")
                else:
                    img = jpeg_for_gemini(jpeg, max_w=512)
                    resp = gemini_model.generate_content([PROMPT, img])
                    raw = (resp.text or "").strip().lower().split()[0].strip(".,;:!?\"'")
                    label = raw
                    code = int(MATERIAL_CODE.get(raw, 0)) or 4
                print(f"   gemini {(time.monotonic()-t1)*1000:.0f} ms → {label} ({code})")
                out.update(
                    {
                        "label": label,
                        "code": code,
                        "raw": label,
                        "visionMaterial": code,
                    }
                )
            else:
                print("   refresh OK")
            set_estado(**out)
            print(f"   total {(time.monotonic()-t0)*1000:.0f} ms")
        except Exception as e:
            print(f"⚠️  {e}")
            set_estado(busy=False, error=str(e))

    # No abrir la cámara al arranque (en algunas USB se cuelga VideoCapture).
    # Se abre en el primer refresh/classify.

    set_estado(busy=False, error=None, label=None, code=0)
    print(f"Polling vision_pi/comando cada {args.poll}s …")
    print(f"Modelo={args.model}  key={'sí' if api_key else 'NO'}")
    sys.stdout.flush()

    try:
        while True:
            now = time.monotonic()
            if now - last_hb >= 30:
                set_estado(online=True)
                last_hb = now

            try:
                snap = cmd_ref.get()
                if snap.exists:
                    data = snap.to_dict() or {}
                    action = (data.get("action") or "").strip().lower()
                    nonce = data.get("nonce")
                    if (
                        action in ("refresh", "classify")
                        and nonce is not None
                        and nonce != last_nonce
                    ):
                        last_nonce = nonce
                        handle(action)
                        cmd_ref.set(
                            {
                                "action": "",
                                "doneNonce": nonce,
                                "at": firestore.SERVER_TIMESTAMP,
                            },
                            merge=True,
                        )
            except Exception as e:
                print(f"⚠️  poll: {e}")

            time.sleep(max(0.2, args.poll))
    except KeyboardInterrupt:
        print("\nBye")
    finally:
        if cam is not None:
            cam.release()
        try:
            estado_ref.set(
                {"online": False, "actualizado": firestore.SERVER_TIMESTAMP},
                merge=True,
            )
        except Exception:
            pass


if __name__ == "__main__":
    main()
