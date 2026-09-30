#!/usr/bin/env python3
"""
SIBU — RFID por CUENTA (vínculo permanente y exclusivo)

ESP32: POST /api/rfid { "uid": "A7240B9F" }

1) Si hay rfid_link_pending/{authUid} vigente → vincula tarjeta (si no es de otra cuenta)
2) Si no, busca rfid_tarjetas/{UID} (preferido) o allowlist UID:Nombre:authUid
3) Escribe rfid_gate/{authUid} { unlocked, untilMs, cardUid, email, nombre }
4) Siempre publica rfid_scan/last para feedback en la web

Una tarjeta vinculada queda PARA SIEMPRE en esa cuenta; otra no puede usarla ni robársela.
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
    o CARDUID:Nombre  (sin cuenta → hay que Vincular una vez en la app)
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
    p.add_argument("--serial", default=os.environ.get("RFID_SERIAL", "") or "")
    p.add_argument("--baud", type=int, default=int(os.environ.get("RFID_BAUD", "115200")))
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
    if args.serial:
        print(f"RFID Serial: {args.serial} @{args.baud} (Arduino USB)")
    sys.stdout.flush()

    def publish_scan(**fields) -> None:
        """Feedback en vivo para la web (cualquier cuenta firmada puede leer)."""
        payload = {
            "atMs": int(time.time() * 1000),
            "at": firestore.SERVER_TIMESTAMP,
            **fields,
        }
        try:
            fs.collection("rfid_scan").document("last").set(payload)
        except Exception as e:
            print(f"⚠️  scan publish: {e}")

    def card_binding(card: str) -> dict | None:
        """Vínculo permanente en Firestore (fuente de verdad)."""
        c = norm_uid(card)
        try:
            snap = fs.collection("rfid_tarjetas").document(c).get()
            if snap.exists:
                d = snap.to_dict() or {}
                if d.get("activa", True) and d.get("authUid"):
                    return {
                        "nombre": str(d.get("nombre") or "Operador"),
                        "email": str(d.get("email") or ""),
                        "authUid": str(d["authUid"]),
                        "cardUid": c,
                    }
        except Exception as e:
            print(f"⚠️  binding: {e}")
        return None

    def lookup_card(card: str) -> dict | None:
        c = norm_uid(card)
        bound = card_binding(c)
        if bound:
            return bound
        if c in allow and allow[c].get("authUid"):
            return {
                "nombre": allow[c]["nombre"],
                "email": "",
                "authUid": allow[c]["authUid"],
                "cardUid": c,
            }
        if c in allow:
            return {"nombre": allow[c]["nombre"], "email": "", "authUid": "", "cardUid": c}
        return None

    def unlock_account(auth_uid: str, nombre: str, card_uid: str, email: str = "") -> dict:
        until_ms = int(time.time() * 1000) + args.ttl * 1000
        payload = {
            "unlocked": True,
            "cardUid": card_uid,
            "nombre": nombre,
            "email": email or "",
            "authUid": auth_uid,
            "untilMs": until_ms,
            "ttlSec": args.ttl,
            "at": firestore.SERVER_TIMESTAMP,
            "lastError": None,
        }
        fs.collection("rfid_gate").document(auth_uid).set(payload, merge=True)
        return payload

    def try_pending_link(card: str) -> tuple[dict | None, str | None]:
        """Vincula tarjeta a la cuenta con pedido vigente."""
        now = int(time.time() * 1000)
        best = None
        best_exp = 0
        for snap in fs.collection("rfid_link_pending").stream():
            d = snap.to_dict() or {}
            if d.get("cancelled") or d.get("done"):
                continue
            auth_uid = str(d.get("authUid") or snap.id or "")
            expires = int(d.get("expiresMs") or 0)
            if not auth_uid:
                continue
            if not expires or now > expires:
                try:
                    snap.reference.delete()
                except Exception:
                    pass
                continue
            if expires >= best_exp:
                best = (
                    snap.reference,
                    auth_uid,
                    str(d.get("nombre") or "Operador"),
                    str(d.get("email") or ""),
                )
                best_exp = expires
        if not best:
            return None, None

        ref, auth_uid, nombre, email = best
        c = norm_uid(card)

        existing = card_binding(c)
        if existing and existing["authUid"] != auth_uid:
            err = (
                f"La tarjeta {c} ya está vinculada a otra cuenta"
                + (f" ({existing.get('email')})" if existing.get("email") else "")
                + ". No se puede reasignar."
            )
            try:
                ref.set(
                    {
                        "error": err,
                        "cardUid": c,
                        "done": False,
                        "at": firestore.SERVER_TIMESTAMP,
                    },
                    merge=True,
                )
            except Exception:
                pass
            print(f"LINK DENY card={c} owned by {existing['authUid']}")
            return None, err

        fs.collection("rfid_tarjetas").document(c).set(
            {
                "activa": True,
                "nombre": nombre,
                "email": email,
                "authUid": auth_uid,
                "vinculadaAt": firestore.SERVER_TIMESTAMP,
                "permanente": True,
            },
            merge=True,
        )
        try:
            ref.set(
                {
                    "done": True,
                    "error": None,
                    "cardUid": c,
                    "email": email,
                    "expiresMs": 0,
                    "at": firestore.SERVER_TIMESTAMP,
                },
                merge=True,
            )
        except Exception:
            try:
                ref.delete()
            except Exception:
                pass
        print(f"LINK card={c} → authUid={auth_uid} email={email}")
        return unlock_account(auth_uid, nombre, c, email), None

    def process_card(card_raw: str) -> dict:
        """Misma lógica para HTTP y Serial Arduino. Retorna dict con ok/code/..."""
        card = norm_uid(card_raw)
        if not card:
            return {"ok": False, "code": 400, "error": "falta uid"}

        linked = None
        link_err = None
        try:
            linked, link_err = try_pending_link(card)
        except Exception as e:
            print(f"⚠️  link: {e}")
            link_err = str(e)

        if linked:
            publish_scan(
                ok=True,
                linked=True,
                cardUid=linked.get("cardUid") or card,
                authUid=linked.get("authUid"),
                email=linked.get("email") or "",
                nombre=linked.get("nombre") or "",
                error=None,
            )
            return {
                "ok": True,
                "code": 200,
                "linked": True,
                "nombre": linked.get("nombre"),
                "email": linked.get("email"),
                "authUid": linked.get("authUid"),
                "cardUid": linked.get("cardUid"),
                "untilMs": linked.get("untilMs"),
                "ttlSec": args.ttl,
            }

        if link_err:
            publish_scan(ok=False, linked=False, cardUid=card, error=link_err)
            return {"ok": False, "code": 403, "error": link_err, "uid": card}

        info = lookup_card(card)
        if not info:
            err = "tarjeta desconocida — tocá Vincular mi tarjeta en la app"
            print(f"DENY unknown card={card}")
            publish_scan(ok=False, cardUid=card, error=err)
            return {"ok": False, "code": 403, "error": err, "uid": card}

        if not info.get("authUid"):
            err = "tarjeta sin cuenta — tocá Vincular mi tarjeta (90 s) y acercá de nuevo"
            print(f"DENY card={info['cardUid']} sin cuenta vinculada")
            publish_scan(ok=False, cardUid=info["cardUid"], error=err, needsLink=True)
            return {"ok": False, "code": 403, "error": err, "uid": info["cardUid"]}

        payload = unlock_account(
            info["authUid"], info["nombre"], info["cardUid"], info.get("email") or ""
        )
        print(f"ALLOW {info['nombre']} card={info['cardUid']} user={info['authUid']}")
        publish_scan(
            ok=True,
            linked=False,
            cardUid=info["cardUid"],
            authUid=info["authUid"],
            email=info.get("email") or "",
            nombre=info["nombre"],
            error=None,
        )
        return {
            "ok": True,
            "code": 200,
            "nombre": info["nombre"],
            "email": info.get("email") or "",
            "authUid": info["authUid"],
            "cardUid": info["cardUid"],
            "untilMs": payload["untilMs"],
            "ttlSec": args.ttl,
        }

    if args.serial:
        import re
        import threading
        from pathlib import Path as _Path

        if not _Path(args.serial).exists():
            print(
                f"⚠️  RFID_SERIAL={args.serial} no existe — "
                "modo solo HTTP (ESP32 WiFi). Vaciá RFID_SERIAL= en sibu.env"
            )
            sys.stdout.flush()
            args.serial = ""
        else:

            def extract_uid_from_line(line: str) -> str:
                line = (line or "").strip().strip("\x00").strip()
                if not line:
                    return ""
                if line.startswith("{"):
                    try:
                        u = json.loads(line).get("uid")
                        return norm_uid(str(u)) if u else ""
                    except Exception:
                        return ""
                # "UID: A7240B9F" (salida del sketch para el IDE)
                m = re.match(r"(?i)^UID\s*[:=]\s*([0-9A-Fa-f:.\-\s]+)\s*$", line)
                if m:
                    return norm_uid(m.group(1))
                # solo hex
                u = norm_uid(line)
                if u and re.fullmatch(r"[0-9A-F]{6,20}", u):
                    return u
                return ""

            def serial_loop() -> None:
                try:
                    import serial  # type: ignore
                except ImportError:
                    print("⚠️  pyserial no instalado — ~/SIBU/.venv/bin/pip install pyserial")
                    sys.stdout.flush()
                    return
                while True:
                    try:
                        if not _Path(args.serial).exists():
                            print(
                                f"⚠️  {args.serial} desapareció — "
                                "sigo solo HTTP hasta que vuelva el puerto"
                            )
                            sys.stdout.flush()
                            time.sleep(10)
                            continue
                        ser = serial.Serial(
                            args.serial,
                            args.baud,
                            timeout=1,
                            write_timeout=1,
                        )
                        # dar tiempo al reset del Arduino al abrir el puerto
                        time.sleep(2.0)
                        ser.reset_input_buffer()
                        print(f"✅ RFID Serial abierto {args.serial}")
                        sys.stdout.flush()
                        while True:
                            raw = ser.readline()
                            if not raw:
                                continue
                            line = raw.decode("utf-8", errors="ignore").strip()
                            if not line:
                                continue
                            print(f"SER << {line}")
                            sys.stdout.flush()
                            uid = extract_uid_from_line(line)
                            if not uid:
                                continue
                            result = process_card(uid)
                            print(
                                f"SER RFID → {result.get('code')} ok={result.get('ok')} "
                                f"{result.get('error') or result.get('cardUid') or ''}"
                            )
                            sys.stdout.flush()
                    except Exception as e:
                        print(f"⚠️  serial RFID: {e} — reintento 3s")
                        sys.stdout.flush()
                        time.sleep(3)

            threading.Thread(target=serial_loop, daemon=True).start()

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
                self._json(
                    200,
                    {
                        "ok": True,
                        "perAccount": True,
                        "exclusive": True,
                        "ttl": args.ttl,
                        "serial": bool(args.serial),
                    },
                )
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
                result = process_card(data.get("uid") or "")
                code = int(result.pop("code", 200))
                self._json(code, result)
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
    print(f"✅ RFID gate (Arduino Serial y/o HTTP) :{args.port}")
    sys.stdout.flush()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nBye")


if __name__ == "__main__":
    main()
