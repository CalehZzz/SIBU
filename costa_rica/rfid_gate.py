#!/usr/bin/env python3
"""
SIBU — RFID gate (ESP32 RC522 → Pi → Firestore → web)

ESP32 hace:
  POST http://IP_PI:8081/api/rfid
  { "uid": "A1B2C3D4" }

La Pi valida contra RFID_ALLOW (env) o Firestore rfid_tarjetas/{uid}
y escribe:
  rfid_gate/sibu { unlocked, uid, nombre, untilMs, at }

La web desbloquea HMI / 📷 / plc-real mientras untilMs > now.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

import firebase_admin
from firebase_admin import credentials, firestore

ROOT = Path(__file__).resolve().parents[1]


def norm_uid(uid: str) -> str:
    u = (uid or "").strip().upper().replace(":", "").replace("-", "").replace(" ", "")
    return u


def load_allow_env() -> dict[str, str]:
    """RFID_ALLOW=A1B2C3D4:Operador1,DEADBEEF:Carla"""
    raw = os.environ.get("RFID_ALLOW", "").strip()
    out: dict[str, str] = {}
    if not raw:
        return out
    for part in raw.split(","):
        part = part.strip()
        if not part:
            continue
        if ":" in part:
            uid, name = part.split(":", 1)
            out[norm_uid(uid)] = name.strip() or "Operador"
        else:
            out[norm_uid(part)] = "Operador"
    return out


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=int(os.environ.get("RFID_PORT", "8081")))
    p.add_argument("--service-account", default=str(ROOT / "serviceAccountKey.json"))
    p.add_argument(
        "--ttl",
        type=int,
        default=int(os.environ.get("RFID_TTL_SEC", "300")),
        help="Segundos de desbloqueo tras tap",
    )
    p.add_argument("--gate-doc", default="sibu")
    args = p.parse_args()

    sa = Path(args.service_account)
    if not sa.is_file():
        print(f"❌ Falta {sa}")
        sys.exit(1)

    if not firebase_admin._apps:
        firebase_admin.initialize_app(credentials.Certificate(str(sa)))
    fs = firestore.client()
    gate_ref = fs.collection("rfid_gate").document(args.gate_doc)
    allow_env = load_allow_env()
    print(f"RFID allow (env): {len(allow_env)} tarjeta(s)")
    print(f"HTTP :{args.port}  TTL={args.ttl}s  doc=rfid_gate/{args.gate_doc}")

    def lookup(uid: str) -> tuple[bool, str]:
        u = norm_uid(uid)
        if not u:
            return False, ""
        if u in allow_env:
            return True, allow_env[u]
        # Firestore rfid_tarjetas/{uid} { activa: true, nombre: "..." }
        try:
            snap = fs.collection("rfid_tarjetas").document(u).get()
            if snap.exists:
                d = snap.to_dict() or {}
                if d.get("activa", True):
                    return True, str(d.get("nombre") or "Operador")
        except Exception as e:
            print(f"⚠️  lookup firestore: {e}")
        return False, ""

    def unlock(uid: str, nombre: str) -> dict:
        until_ms = int(time.time() * 1000) + args.ttl * 1000
        payload = {
            "unlocked": True,
            "uid": norm_uid(uid),
            "nombre": nombre,
            "untilMs": until_ms,
            "ttlSec": args.ttl,
            "at": firestore.SERVER_TIMESTAMP,
        }
        gate_ref.set(payload, merge=True)
        return payload

    def lock() -> None:
        gate_ref.set(
            {
                "unlocked": False,
                "uid": None,
                "nombre": None,
                "untilMs": 0,
                "at": firestore.SERVER_TIMESTAMP,
            },
            merge=True,
        )

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
            path = urlparse(self.path).path
            if path == "/api/health":
                self._json(200, {"ok": True, "service": "rfid_gate", "ttl": args.ttl})
                return
            if path == "/api/status":
                snap = gate_ref.get()
                d = snap.to_dict() if snap.exists else {}
                self._json(200, {"ok": True, "gate": d})
                return
            self._json(404, {"ok": False, "error": "not found"})

        def do_POST(self) -> None:
            path = urlparse(self.path).path
            n = int(self.headers.get("Content-Length") or 0)
            body = self.rfile.read(n) if n else b"{}"
            try:
                data = json.loads(body.decode("utf-8") or "{}")
            except Exception:
                self._json(400, {"ok": False, "error": "JSON inválido"})
                return

            if path == "/api/rfid":
                uid = data.get("uid") or data.get("UID") or ""
                ok, nombre = lookup(uid)
                if not ok:
                    print(f"DENY uid={norm_uid(uid)}")
                    self._json(403, {"ok": False, "error": "tarjeta no autorizada", "uid": norm_uid(uid)})
                    return
                payload = unlock(uid, nombre)
                print(f"ALLOW {nombre} uid={norm_uid(uid)} untilMs={payload['untilMs']}")
                self._json(
                    200,
                    {
                        "ok": True,
                        "nombre": nombre,
                        "uid": norm_uid(uid),
                        "untilMs": payload["untilMs"],
                        "ttlSec": args.ttl,
                    },
                )
                return

            if path == "/api/lock":
                lock()
                self._json(200, {"ok": True, "unlocked": False})
                return

            self._json(404, {"ok": False, "error": "not found"})

    # Estado inicial: no bloquees el bind si Firestore falla
    try:
        lock()
        print("Firestore gate → locked")
    except Exception as e:
        print(f"⚠️  no se pudo lock inicial en Firestore: {e}")

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"✅ RFID gate ESCUCHANDO http://0.0.0.0:{args.port}  POST /api/rfid")
    sys.stdout.flush()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nBye")


if __name__ == "__main__":
    main()
