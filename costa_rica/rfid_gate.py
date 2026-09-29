#!/usr/bin/env python3
"""
SIBU — RFID por CUENTA (no global)

ESP32: POST /api/rfid { "uid": "A7240B9F" }

1) Si hay rfid_link_pending/{authUid} vigente → vincula tarjeta a ese authUid
2) Si no, busca rfid_tarjetas/{UID} o allowlist UID:Nombre:authUid
3) Escribe rfid_gate/{authUid} { unlocked, untilMs, cardUid, nombre }

Cada usuario de Google solo desbloquea SU documento.
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
    return u.strip('"').strip("'")


def load_allow() -> dict[str, dict]:
    """
    Línea: CARDUID:Nombre:authUid
    o CARDUID:Nombre  (sin cuenta → solo válida tras vincular en app)
    """
    out: dict[str, dict] = {}

    def add_line(part: str) -> None:
        part = part.strip().strip('"').strip("'")
        if not part or part.startswith("#"):
            return
        bits = [b.strip() for b in part.split(":")]
        if not bits:
            return
        card = norm_uid(bits[0])
        nombre = bits[1] if len(bits) > 1 else "Operador"
        auth = bits[2] if len(bits) > 2 else ""
        out[card] = {"nombre": nombre or "Operador", "authUid": auth}

    raw = (os.environ.get("RFID_ALLOW", "") or "").strip().strip('"').strip("'")
    for part in raw.replace(";", ",").split(","):
        add_line(part)

    allow_file = ROOT / "costa_rica" / "rfid_allow.txt"
    if allow_file.is_file():
        for line in allow_file.read_text(encoding="utf-8").splitlines():
            add_line(line)
    return out


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=int(os.environ.get("RFID_PORT", "8081")))
    p.add_argument("--service-account", default=str(ROOT / "serviceAccountKey.json"))
    p.add_argument("--ttl", type=int, default=int(os.environ.get("RFID_TTL_SEC", "300")))
    args = p.parse_args()

    sa = Path(args.service_account)
    if not sa.is_file():
        print(f"❌ Falta {sa}")
        sys.exit(1)

    if not firebase_admin._apps:
        firebase_admin.initialize_app(credentials.Certificate(str(sa)))
    fs = firestore.client()
    allow = load_allow()
    print(f"RFID allow file/env: {len(allow)} → {list(allow.keys())}")
    sys.stdout.flush()

    def lookup_card(card: str) -> dict | None:
        c = norm_uid(card)
        if c in allow and allow[c].get("authUid"):
            return {"nombre": allow[c]["nombre"], "authUid": allow[c]["authUid"], "cardUid": c}
        try:
            snap = fs.collection("rfid_tarjetas").document(c).get()
            if snap.exists:
                d = snap.to_dict() or {}
                if d.get("activa", True) and d.get("authUid"):
                    return {
                        "nombre": str(d.get("nombre") or "Operador"),
                        "authUid": str(d["authUid"]),
                        "cardUid": c,
                    }
        except Exception as e:
            print(f"⚠️  lookup: {e}")
        # allow sin authUid → conocida pero no vinculada
        if c in allow:
            return {"nombre": allow[c]["nombre"], "authUid": "", "cardUid": c}
        return None

    def unlock_account(auth_uid: str, nombre: str, card_uid: str) -> dict:
        until_ms = int(time.time() * 1000) + args.ttl * 1000
        payload = {
            "unlocked": True,
            "cardUid": card_uid,
            "nombre": nombre,
            "authUid": auth_uid,
            "untilMs": until_ms,
            "ttlSec": args.ttl,
            "at": firestore.SERVER_TIMESTAMP,
        }
        fs.collection("rfid_gate").document(auth_uid).set(payload, merge=True)
        return payload

    def try_pending_link(card: str) -> dict | None:
        """Si alguien en la web pidió vincular, asigna esta tarjeta a SU cuenta."""
        now = int(time.time() * 1000)
        best = None
        best_exp = 0
        for snap in fs.collection("rfid_link_pending").stream():
            d = snap.to_dict() or {}
            auth_uid = str(d.get("authUid") or snap.id or "")
            expires = int(d.get("expiresMs") or 0)
            if not auth_uid:
                continue
            if expires and now > expires:
                try:
                    snap.reference.delete()
                except Exception:
                    pass
                continue
            # el más reciente (mayor expiresMs) gana si hay varios
            if expires >= best_exp:
                best = (snap.reference, auth_uid, str(d.get("nombre") or "Operador"))
                best_exp = expires
        if not best:
            return None
        ref, auth_uid, nombre = best
        c = norm_uid(card)
        fs.collection("rfid_tarjetas").document(c).set(
            {
                "activa": True,
                "nombre": nombre,
                "authUid": auth_uid,
                "vinculadaAt": firestore.SERVER_TIMESTAMP,
            },
            merge=True,
        )
        try:
            ref.delete()
        except Exception:
            pass
        print(f"LINK card={c} → authUid={auth_uid}")
        return unlock_account(auth_uid, nombre, c)

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
                self._json(200, {"ok": True, "perAccount": True, "ttl": args.ttl})
                return
            self._json(404, {"ok": False})

        def do_POST(self) -> None:
            path = urlparse(self.path).path
            n = int(self.headers.get("Content-Length") or 0)
            try:
                data = json.loads((self.rfile.read(n) if n else b"{}").decode("utf-8") or "{}")
            except Exception:
                self._json(400, {"ok": False, "error": "JSON inválido"})
                return

            if path == "/api/rfid":
                card = data.get("uid") or ""
                # 1) vincular pendiente
                try:
                    linked = try_pending_link(card)
                except Exception as e:
                    print(f"⚠️  link: {e}")
                    linked = None
                if linked:
                    self._json(200, {"ok": True, "linked": True, **{k: linked[k] for k in ("nombre", "untilMs", "ttlSec") if k in linked}, "authUid": linked.get("authUid"), "cardUid": linked.get("cardUid")})
                    return

                info = lookup_card(card)
                if not info:
                    print(f"DENY unknown card={norm_uid(card)}")
                    self._json(403, {"ok": False, "error": "tarjeta desconocida", "uid": norm_uid(card)})
                    return
                if not info.get("authUid"):
                    print(f"DENY card={info['cardUid']} sin cuenta vinculada")
                    self._json(
                        403,
                        {
                            "ok": False,
                            "error": "tarjeta sin cuenta: vinculá desde la app (botón Vincular)",
                            "uid": info["cardUid"],
                        },
                    )
                    return
                payload = unlock_account(info["authUid"], info["nombre"], info["cardUid"])
                print(f"ALLOW {info['nombre']} card={info['cardUid']} user={info['authUid']}")
                self._json(
                    200,
                    {
                        "ok": True,
                        "nombre": info["nombre"],
                        "authUid": info["authUid"],
                        "cardUid": info["cardUid"],
                        "untilMs": payload["untilMs"],
                        "ttlSec": args.ttl,
                    },
                )
                return

            if path == "/api/lock":
                auth_uid = str(data.get("authUid") or "")
                if not auth_uid:
                    self._json(400, {"ok": False, "error": "falta authUid"})
                    return
                fs.collection("rfid_gate").document(auth_uid).set(
                    {
                        "unlocked": False,
                        "untilMs": 0,
                        "cardUid": None,
                        "at": firestore.SERVER_TIMESTAMP,
                    },
                    merge=True,
                )
                self._json(200, {"ok": True})
                return

            self._json(404, {"ok": False})

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"✅ RFID gate (por cuenta) :{args.port}")
    sys.stdout.flush()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nBye")


if __name__ == "__main__":
    main()
