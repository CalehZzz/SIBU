#!/usr/bin/env python3
"""
SIBU — HTTP de visión en la Raspberry Pi (SIN PLC)

Sirve:
  GET  /api/health     → {"ok": true}
  GET  /api/snapshot   → JPEG de la cámara de la Pi
  POST /api/classify   → captura + Gemini → {label, code, raw}
  GET  /               → index.html del repo (misma origen = sin CORS)

Uso en la Pi:
  export GEMINI_API_KEY=...
  cd ~/SIBU
  source .venv/bin/activate
  python costa_rica/vision_http.py --port 8080

Desde el celular/PC (misma red):
  http://IP_DE_LA_PI:8080/   → abrí SIBU y andá a 📷
"""

from __future__ import annotations

import argparse
import json
import mimetypes
import os
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
PHOTO_DEFAULT = "/tmp/sibu_web_shot.jpg"

MATERIAL_CODE = {
    "plastico": 1,
    "plastic": 1,
    "aluminio": 2,
    "lata": 2,
    "latas": 2,
    "vidrio": 3,
    "glass": 3,
}

PROMPT = (
    "Clasifica el objeto reciclable en la imagen. "
    "Responde UNA sola palabra en minúsculas, sin puntuación: "
    "plastico | aluminio | vidrio | desconocido"
)

_lock = threading.Lock()


def capturar(path: str, capture_cmd: str) -> None:
    if capture_cmd:
        cmd = capture_cmd.format(path=path)
        subprocess.run(cmd, shell=True, check=True)
        return
    try:
        import cv2  # type: ignore
    except ImportError as e:
        raise RuntimeError(
            "Definí --capture-cmd (ej. libcamera-still -n -t 1 -o {path}) "
            "o instalá opencv-python"
        ) from e
    cam = cv2.VideoCapture(0)
    ok, frame = cam.read()
    cam.release()
    if not ok:
        raise RuntimeError("No se pudo leer la cámara (/dev/video0)")
    cv2.imwrite(path, frame)


def clasificar_gemini(path: str, model_name: str, api_key: str) -> str:
    import google.generativeai as genai
    from PIL import Image

    genai.configure(api_key=api_key)
    model = genai.GenerativeModel(model_name)
    img = Image.open(path)
    resp = model.generate_content([PROMPT, img])
    text = (resp.text or "").strip().lower().split()[0]
    return text.strip(".,;:!?\"'")


def codigo_de(label: str) -> int:
    return int(MATERIAL_CODE.get(label, 0))


class VisionHandler(BaseHTTPRequestHandler):
    server_version = "SIBUVision/1.0"

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _cors(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _json(self, code: int, obj: dict) -> None:
        raw = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self._cors()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def _bytes(self, code: int, data: bytes, content_type: str) -> None:
        self.send_response(code)
        self._cors()
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self) -> None:
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        cfg = self.server.cfg  # type: ignore[attr-defined]

        if path == "/api/health":
            self._json(
                200,
                {
                    "ok": True,
                    "plc": False,
                    "model": cfg["model"],
                    "has_key": bool(cfg["api_key"]),
                },
            )
            return

        if path == "/api/snapshot":
            try:
                with _lock:
                    capturar(cfg["photo_path"], cfg["capture_cmd"])
                    data = Path(cfg["photo_path"]).read_bytes()
                self._bytes(200, data, "image/jpeg")
            except Exception as e:
                self._json(500, {"ok": False, "error": str(e)})
            return

        # Archivos estáticos del repo (para abrir la web desde la Pi)
        rel = "index.html" if path in ("/", "/index.html") else path.lstrip("/")
        if ".." in rel or rel.startswith("/"):
            self._json(400, {"ok": False, "error": "path inválido"})
            return
        file_path = ROOT / rel
        if not file_path.is_file():
            self._json(404, {"ok": False, "error": "no encontrado", "path": rel})
            return
        data = file_path.read_bytes()
        ctype = mimetypes.guess_type(str(file_path))[0] or "application/octet-stream"
        self._bytes(200, data, ctype)

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        cfg = self.server.cfg  # type: ignore[attr-defined]

        if path != "/api/classify":
            self._json(404, {"ok": False, "error": "ruta desconocida"})
            return

        if not cfg["api_key"] and not cfg["mock"]:
            self._json(
                400,
                {
                    "ok": False,
                    "error": "Falta GEMINI_API_KEY en la Pi (export …)",
                },
            )
            return

        try:
            with _lock:
                capturar(cfg["photo_path"], cfg["capture_cmd"])
                if cfg["mock"]:
                    label = cfg["mock"]
                else:
                    label = clasificar_gemini(
                        cfg["photo_path"], cfg["model"], cfg["api_key"]
                    )
                code = codigo_de(label)
            # devolver también miniatura en base64 opcional: omitimos, el front pide snapshot
            self._json(
                200,
                {
                    "ok": True,
                    "label": label,
                    "code": code,
                    "raw": label,
                    "visionMaterial": code,
                },
            )
        except Exception as e:
            self._json(500, {"ok": False, "error": str(e)})


def main() -> None:
    p = argparse.ArgumentParser(description="Pi · HTTP visión Gemini (sin PLC)")
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=8080)
    p.add_argument(
        "--capture-cmd",
        default=os.environ.get("SIBU_CAPTURE_CMD", ""),
        help='Ej: libcamera-still -n -t 1 -o {path}',
    )
    p.add_argument("--photo-path", default=PHOTO_DEFAULT)
    p.add_argument("--model", default="gemini-2.0-flash")
    p.add_argument(
        "--mock-material",
        default="",
        help="Sin Gemini: plastico|aluminio|vidrio",
    )
    args = p.parse_args()

    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    mock = args.mock_material.strip().lower()
    if not api_key and not mock:
        print("⚠️  Sin GEMINI_API_KEY — usá export o --mock-material")

    httpd = ThreadingHTTPServer((args.host, args.port), VisionHandler)
    httpd.cfg = {
        "api_key": api_key,
        "model": args.model,
        "capture_cmd": args.capture_cmd,
        "photo_path": args.photo_path,
        "mock": mock,
    }

    print(f"SIBU visión HTTP en http://0.0.0.0:{args.port}")
    print(f"  Abrí en el celular: http://IP_DE_LA_PI:{args.port}/")
    print("  Endpoints: /api/health  /api/snapshot  /api/classify")
    print("  PLC: no requerido")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nBye")


if __name__ == "__main__":
    main()
