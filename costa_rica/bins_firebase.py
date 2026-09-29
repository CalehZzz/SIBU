#!/usr/bin/env python3
"""
SIBU — Niveles de botes (Arduino/ESP HC-SR04) → Firestore bins_pi/estado

POST /api/bins  JSON del sketch
GET  /api/health

Opcional: --serial /dev/ttyUSB0  (Arduino Uno por USB)
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

import firebase_admin
from firebase_admin import credentials, firestore

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=int(os.environ.get("BINS_PORT", "8082")))
    p.add_argument("--service-account", default=str(ROOT / "serviceAccountKey.json"))
    p.add_argument("--serial", default=os.environ.get("BINS_SERIAL", "") or "")
    p.add_argument("--baud", type=int, default=int(os.environ.get("BINS_BAUD", "115200")))
    args = p.parse_args()

    sa = Path(args.service_account)
    if not sa.is_file():
        print(f"❌ Falta {sa}")
        sys.exit(1)
    if not firebase_admin._apps:
        firebase_admin.initialize_app(credentials.Certificate(str(sa)))
    fs = firestore.client()

    def publish(data: dict) -> None:
        payload = {
            **data,
            "online": True,
            "at": firestore.SERVER_TIMESTAMP,
            "atMs": int(time.time() * 1000),
        }
        fs.collection("bins_pi").document("estado").set(payload, merge=True)

    def normalize(raw: dict) -> dict:
        def bin_of(key: str) -> dict:
            b = raw.get(key) or {}
            return {
                "cm": float(b["cm"]) if b.get("cm") is not None else None,
                "pct": int(b.get("pct") or 0),
                "lleno": bool(b.get("lleno")),
            }

        divert = raw.get("divert") or {}
        return {
            "plastico": bin_of("plastico"),
            "aluminio": bin_of("aluminio"),
            "vidrio": bin_of("vidrio"),
            "rechazo": bin_of("rechazo"),
            "lamp": str(raw.get("lamp") or "green"),
            "stop": bool(raw.get("stop")),
            "divert": {
                "plastico": bool(divert.get("plastico")),
                "aluminio": bool(divert.get("aluminio")),
                "vidrio": bool(divert.get("vidrio")),
            },
        }

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *a) -> None:
            sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % a))

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

        def do_OPTIONS(self) -> None:
            self.send_response(204)
            self._cors()
            self.end_headers()

        def do_GET(self) -> None:
            if urlparse(self.path).path == "/api/health":
                self._json(200, {"ok": True, "bins": True})
                return
            self._json(404, {"ok": False})

        def do_POST(self) -> None:
            if urlparse(self.path).path != "/api/bins":
                self._json(404, {"ok": False})
                return
            n = int(self.headers.get("Content-Length") or 0)
            try:
                data = json.loads((self.rfile.read(n) if n else b"{}").decode("utf-8") or "{}")
                norm = normalize(data)
                publish(norm)
                lamp = norm["lamp"]
                print(f"BINS lamp={lamp} stop={norm['stop']} P={norm['plastico']['pct']}% A={norm['aluminio']['pct']}% V={norm['vidrio']['pct']}% R={norm['rechazo']['pct']}%")
                self._json(200, {"ok": True})
            except Exception as e:
                print(f"⚠️  bins: {e}")
                self._json(400, {"ok": False, "error": str(e)})

    if args.serial:
        def serial_loop() -> None:
            try:
                import serial  # type: ignore
            except ImportError:
                print("⚠️  pyserial no instalado — pip install pyserial")
                return
            while True:
                try:
                    ser = serial.Serial(args.serial, args.baud, timeout=1)
                    print(f"Serial bins {args.serial} @{args.baud}")
                    while True:
                        line = ser.readline().decode("utf-8", errors="ignore").strip()
                        if not line.startswith("{"):
                            continue
                        try:
                            norm = normalize(json.loads(line))
                            publish(norm)
                            print(f"SER lamp={norm['lamp']} stop={norm['stop']}")
                        except Exception as e:
                            print(f"⚠️  serial parse: {e}")
                except Exception as e:
                    print(f"⚠️  serial: {e} — reintento 3s")
                    time.sleep(3)

        threading.Thread(target=serial_loop, daemon=True).start()

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"✅ Bins gate :{args.port}")
    sys.stdout.flush()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nBye")


if __name__ == "__main__":
    main()
