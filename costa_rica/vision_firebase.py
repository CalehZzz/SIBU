#!/usr/bin/env python3
"""
SIBU — Visión Pi ↔ Firestore (página web, sin :8080)

  Página → vision_pi/comando { action: refresh|classify, nonce }
  Pi     → vision_pi/estado  { online, jpegBase64, label, code, busy, error }

Usa POLLING (fiable). ~1 lectura / 2 s de `comando` (bajo uso).
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
PHOTO_DEFAULT = "/tmp/sibu_fb_shot.jpg"

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


def capturar(path: str, capture_cmd: str) -> None:
    import subprocess

    if capture_cmd:
        subprocess.run(capture_cmd.format(path=path), shell=True, check=True)
        return
    import cv2  # type: ignore

    cam = cv2.VideoCapture(0)
    ok, frame = cam.read()
    cam.release()
    if not ok:
        raise RuntimeError("No se pudo leer la cámara USB (/dev/video0)")
    cv2.imwrite(path, frame)


def jpeg_b64_thumb(path: str, max_w: int = 640, quality: int = 55) -> str:
    from PIL import Image

    img = Image.open(path).convert("RGB")
    w, h = img.size
    if w > max_w:
        img = img.resize((max_w, int(h * max_w / w)))
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=quality, optimize=True)
    return base64.b64encode(buf.getvalue()).decode("ascii")


def clasificar(path: str, model: str, api_key: str) -> tuple[str, int]:
    import google.generativeai as genai
    from PIL import Image

    genai.configure(api_key=api_key)
    model_g = genai.GenerativeModel(model)
    resp = model_g.generate_content([PROMPT, Image.open(path)])
    raw = (resp.text or "").strip().lower().split()[0].strip(".,;:!?\"'")
    code = int(MATERIAL_CODE.get(raw, 0)) or 4
    return raw, code


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--service-account", default=str(ROOT / "serviceAccountKey.json"))
    p.add_argument("--photo-path", default=PHOTO_DEFAULT)
    p.add_argument("--capture-cmd", default=os.environ.get("SIBU_CAPTURE_CMD", ""))
    p.add_argument("--model", default=os.environ.get("GEMINI_MODEL", "gemini-3.8-flash"))
    p.add_argument("--mock-material", default="")
    p.add_argument("--poll", type=float, default=2.0, help="Segundos entre lecturas de comando")
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

    def handle(action: str) -> None:
        print(f"→ acción: {action}")
        set_estado(busy=True, error=None)
        try:
            capturar(args.photo_path, args.capture_cmd)
            b64 = jpeg_b64_thumb(args.photo_path)
            out = {"jpegBase64": b64, "busy": False, "error": None}
            if action == "classify":
                if mock:
                    label, code = mock, int(MATERIAL_CODE.get(mock, 4))
                elif not api_key:
                    raise RuntimeError("Falta GEMINI_API_KEY en la Pi")
                else:
                    label, code = clasificar(args.photo_path, args.model, api_key)
                out.update(
                    {
                        "label": label,
                        "code": code,
                        "raw": label,
                        "visionMaterial": code,
                    }
                )
                print(f"   classify → {label} ({code})")
            else:
                print("   refresh OK")
            set_estado(**out)
        except Exception as e:
            print(f"⚠️  {e}")
            set_estado(busy=False, error=str(e))

    set_estado(busy=False, error=None, label=None, code=0)
    print(f"Polling vision_pi/comando cada {args.poll}s …")
    print(f"Modelo={args.model}  key={'sí' if api_key else 'NO'}")

    try:
        while True:
            now = time.monotonic()
            if now - last_hb >= 30:
                set_estado(online=True)
                last_hb = now
                print("heartbeat online")

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
                        # limpia para no reprocesar
                        cmd_ref.set(
                            {"action": "", "doneNonce": nonce, "at": firestore.SERVER_TIMESTAMP},
                            merge=True,
                        )
            except Exception as e:
                print(f"⚠️  poll: {e}")

            time.sleep(max(0.5, args.poll))
    except KeyboardInterrupt:
        print("\nBye")
        try:
            estado_ref.set(
                {"online": False, "actualizado": firestore.SERVER_TIMESTAMP},
                merge=True,
            )
        except Exception:
            pass


if __name__ == "__main__":
    main()
