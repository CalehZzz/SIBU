#!/usr/bin/env python3
"""
SIBU — Visión Pi ↔ Firestore (para la página web en cualquier red)

La web (Firebase Hosting / celular) NO necesita http://IP:8080.
  - Página escribe  vision_pi/comando  { action: refresh|classify, nonce }
  - Este servicio (Admin SDK) captura cámara + opcional Gemini
  - Escribe      vision_pi/estado   { online, jpegBase64, label, code, raw, busy, error, ... }

Uso:
  export GEMINI_API_KEY=...
  cd ~/SIBU && source .venv/bin/activate
  python costa_rica/vision_firebase.py

Casi no gasta cuota: escucha on_snapshot (solo lee cuando cambia el comando).
"""

from __future__ import annotations

import argparse
import base64
import io
import os
import sys
import threading
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
    args = p.parse_args()

    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    mock = args.mock_material.strip().lower()
    if not api_key and not mock:
        print("⚠️  Sin GEMINI_API_KEY (classify fallará; refresh sí funciona)")

    if not firebase_admin._apps:
        firebase_admin.initialize_app(credentials.Certificate(args.service_account))
    fs = firestore.client()
    estado_ref = fs.collection("vision_pi").document("estado")
    cmd_ref = fs.collection("vision_pi").document("comando")

    lock = threading.Lock()
    last_nonce = None

    def set_estado(**fields):
        payload = {
            "online": True,
            "pi": True,
            "actualizado": firestore.SERVER_TIMESTAMP,
            **fields,
        }
        estado_ref.set(payload, merge=True)

    def handle(action: str):
        with lock:
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
                    print(f"classify → {label} ({code})")
                else:
                    print("refresh snapshot OK")
                set_estado(**out)
            except Exception as e:
                print(f"⚠️  {e}")
                set_estado(busy=False, error=str(e))

    def on_cmd(doc_snap, changes, read_time):
        nonlocal last_nonce
        if not doc_snap.exists:
            return
        data = doc_snap.to_dict() or {}
        action = (data.get("action") or "").strip().lower()
        nonce = data.get("nonce")
        if action not in ("refresh", "classify"):
            return
        if nonce is not None and nonce == last_nonce:
            return
        last_nonce = nonce
        # procesar fuera del callback
        threading.Thread(target=handle, args=(action,), daemon=True).start()

    set_estado(busy=False, error=None, label=None, code=0)
    cmd_ref.on_snapshot(on_cmd)
    print("Escuchando vision_pi/comando … (Ctrl+C sale)")
    print("La web SIBU (📷) puede usarse desde Firebase Hosting, sin :8080")

    try:
        while True:
            # heartbeat online barato (1 write / 60 s)
            set_estado(online=True)
            time.sleep(60)
    except KeyboardInterrupt:
        print("\nBye")
        try:
            estado_ref.set({"online": False, "actualizado": firestore.SERVER_TIMESTAMP}, merge=True)
        except Exception:
            pass


if __name__ == "__main__":
    main()
