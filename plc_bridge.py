"""
GUACAMAYOS — Bridge bidireccional
  Firestore hmi_comandos → DB_HMI (PLC)
  DatosEstacion (PLC) → Firestore sesiones_activas

CPU objetivo: 1511C-1 PN en S7-PLCSIM Advanced V7 (snap7 a la IP de la instancia).

  pip install firebase-admin python-snap7
  python plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
"""

from __future__ import annotations

import argparse
import sys
import time

import firebase_admin
from firebase_admin import credentials, firestore

try:
    import snap7
    from snap7.util import get_bool, get_int, get_real, set_bool, set_real
except ImportError:
    print("Falta python-snap7. Instala con: pip install python-snap7")
    sys.exit(1)


SERVICE_ACCOUNT_PATH = "serviceAccountKey.json"
# DatosEstacion termina en ContVidrio@22 + PesoVidrioKg@24 → 28 bytes.
DB_READ_SIZE = 28
# DB_HMI: bools 0..1 + Real PesoActualKg @ 2.0 + VisionMaterial Int @ 6.0 → 8 bytes.
# El bridge HMI solo ESCRIBE bytes 0..5 (no pisa VisionMaterial; eso lo escribe la Pi/Gemini).
DB_HMI_SIZE = 8
DB_HMI_WRITE_SIZE = 6
PESO_OFFSET = 2  # si tu DB_HMI muestra otro offset al compilar, cámbialo aquí
VISION_OFFSET = 6  # Int: 0 ninguno · 1 plástico · 2 aluminio · 3 vidrio

# Tamaños de fallback si el DB en el PLC aún no está ampliado (TIA Download pendiente).
DB_READ_FALLBACKS = (28, 24, 22, 20, 18)
DB_HMI_WRITE_FALLBACKS = (6, 2)

ESTADO_TXT = {0: "idle", 1: "running", 2: "clasificando", 3: "alarma", 4: "emergencia"}
MATERIAL_TXT = {0: None, 1: "plastico", 2: "aluminio", 3: "vidrio", 4: "desconocido"}

# Avisos repetidos (Invalid address / DB chico) → una vez + recordatorio cada N s
_warn_state: dict[str, float] = {}
_WARN_REMIND_S = 30.0


def _warn_once(key: str, msg: str, *, remind_s: float = _WARN_REMIND_S) -> None:
    now = time.monotonic()
    last = _warn_state.get(key)
    if last is not None and (now - last) < remind_s:
        return
    _warn_state[key] = now
    print(msg)


def _db_max_readable(client: snap7.client.Client, dbn: int, sizes: tuple[int, ...]) -> int:
    max_ok = 0
    for sz in sizes:
        try:
            client.db_read(dbn, 0, sz)
            max_ok = sz
        except Exception:
            break
    return max_ok

BOOL_MAP = [
    # (firestore_key, byte, bit)
    ("Start", 0, 0),
    ("Stop", 0, 1),
    ("Emergencia", 0, 2),
    ("ResetAlarma", 0, 3),
    ("ModoAuto", 0, 4),
    ("FinSesion", 0, 5),
    ("ManualBanda", 0, 6),
    ("ManualPiston", 0, 7),       # manual P3 vidrio
    ("BasculaLista", 1, 0),
    ("SensorPieza", 1, 1),
    ("SensorPlastico", 1, 2),
    ("SensorAluminio", 1, 3),     # latas
    ("ManualPiston1", 1, 4),      # manual P1 plástico
    ("ManualPiston2", 1, 5),      # manual P2 latas
    ("SensorVidrio", 1, 6),       # sim / real → I_SensorVidrio
]


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Bridge Guacamayos HMI↔PLC↔Firestore")
    p.add_argument("estacion_id", help="ID estación Firestore")
    p.add_argument("--ip", default="192.168.0.1", help="IP instancia PLCSIM Advanced")
    p.add_argument("--rack", type=int, default=0)
    p.add_argument("--slot", type=int, default=1)
    p.add_argument("--db", type=int, default=1, help="DB DatosEstacion")
    p.add_argument("--db-hmi", type=int, default=3, help="DB_HMI comandos")
    p.add_argument("--interval", type=float, default=0.25)
    p.add_argument("--reset-on-start", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--no-hmi-write", action="store_true", help="Solo lee PLC, no escribe DB_HMI")
    p.add_argument(
        "--pulse-hold",
        type=float,
        default=1.0,
        help="Segundos que Start/Stop/Reset/FinSesion se mantienen en 1 en el PLC",
    )
    return p.parse_args()


# Flancos cortos desde Firestore: el bridge los alarga para que el LAD los vea.
PULSE_KEYS = ("Start", "Stop", "ResetAlarma", "FinSesion")
_pulse_hold_until: dict[str, float] = {}
_pulse_prev: dict[str, bool] = {k: False for k in PULSE_KEYS}


def aplicar_retencion_pulsos(cmd: dict, hold_s: float) -> dict:
    """Si Start/Stop aparece en 1 (o flanco), lo mantiene hold_s en el DB del PLC."""
    out = dict(cmd)
    now = time.monotonic()

    for key in PULSE_KEYS:
        on = bool(cmd.get(key))
        rising = on and not _pulse_prev.get(key, False)
        _pulse_prev[key] = on
        if rising or on:
            _pulse_hold_until[key] = now + max(0.2, hold_s)

    # Start y Stop no a la vez (LAD: Start con NC Stop)
    if _pulse_hold_until.get("Stop", 0) > now:
        _pulse_hold_until["Start"] = 0
    if _pulse_hold_until.get("Start", 0) > now and not (
        _pulse_hold_until.get("Stop", 0) > now
    ):
        pass

    for key in PULSE_KEYS:
        until = _pulse_hold_until.get(key, 0)
        if until > now:
            out[key] = True
        else:
            out[key] = bool(cmd.get(key))
            _pulse_hold_until.pop(key, None)

    if out.get("Start") and out.get("Stop"):
        # Preferir el más reciente
        if _pulse_hold_until.get("Stop", 0) >= _pulse_hold_until.get("Start", 0):
            out["Start"] = False
        else:
            out["Stop"] = False
    return out

def conectar_plc(ip: str, rack: int, slot: int) -> snap7.client.Client:
    # Tipo OP (3) suele ir mejor con S7-1200/1500 + PUT/GET que el PG por defecto.
    client = snap7.client.Client()
    try:
        client.set_connection_type(3)  # 1=PG, 2=OP, 3=Basic/OP
    except Exception:
        pass
    client.connect(ip, rack, slot)
    if not client.get_connected():
        raise RuntimeError(f"No conectó a {ip} r{rack}s{slot}")
    return client


def diagnostico_dbs(client: snap7.client.Client, db_datos: int, db_hmi: int) -> None:
    """Si falla, explica la causa más probable (0x05)."""
    print("\n--- Diagnóstico rápido DB ---")
    try:
        client.mb_read(0, 2)
        print("  Merker MB0: OK (comunicación PUT/GET básica responde)")
    except Exception as e:
        print(f"  Merker MB0: FAIL ({e})")
        print("  → Download HARDWARE de la CPU con PUT/GET a esta instancia PLCSIM.")

    probe_sizes = (2, 4, 6, 7, 8, 16, 20, 22, 24, 28, 32)
    for dbn, label, need in ((db_datos, "DatosEstacion", DB_READ_SIZE), (db_hmi, "DB_HMI", DB_HMI_SIZE)):
        max_ok = _db_max_readable(client, dbn, probe_sizes)
        if max_ok >= need:
            print(f"  DB{dbn} ({label}): OK (legible ≥{max_ok} bytes, necesitas {need})")
        elif max_ok > 0:
            print(f"  DB{dbn} ({label}): parcial — solo {max_ok} bytes (necesitas {need})")
            print("  → El DB es demasiado pequeño: agrega todos los campos y vuelve a descargar.")
            if label == "DB_HMI":
                print(
                    "  → Campos: ManualPiston1/2 @1.4/1.5 · SensorVidrio @1.6 · "
                    "PesoActualKg @2.0 · VisionMaterial Int @6.0 (total 8 B)"
                )
            else:
                print("  → Campos: ContVidrio Int @22 + PesoVidrioKg Real @24 (total 28 B)")
        else:
            print(f"  DB{dbn} ({label}): NO legible (Invalid address 0x05 típico)")
            print("  → Causas: DB no descargado en ESTA IP, número distinto, o Optimized ON.")
    print("  Tip: py plc_probe.py --ip <misma_IP>  → lista todos los DB visibles")
    print("  Guía: plc_real/FIX_DB_INVALID_ADDRESS.md\n")


def leer_datos_estacion(client: snap7.client.Client, db_number: int) -> dict:
    raw = None
    used = 0
    last_err: Exception | None = None
    for sz in DB_READ_FALLBACKS:
        try:
            raw = client.db_read(db_number, 0, sz)
            used = sz
            break
        except Exception as e:
            last_err = e
    if raw is None:
        raise RuntimeError(
            f"Read DatosEstacion DB{db_number} falló (Invalid address 0x05). "
            "En TIA: DB1 Optimized OFF, ≥28 bytes (ContVidrio@22 + PesoVidrioKg@24), "
            "Download software a ESTA IP. Ver plc_real/FIX_DB_INVALID_ADDRESS.md"
        ) from last_err

    if used < DB_READ_SIZE:
        _warn_once(
            "datos_parcial",
            f"⚠️  DatosEstacion DB{db_number}: solo {used} B legibles (ideal {DB_READ_SIZE}). "
            "Agrega ContVidrio@22 + PesoVidrioKg@24 → Download. "
            "Mientras tanto el bridge lee lo que haya.",
        )

    estado = get_int(raw, 18) if used >= 20 else 0
    ultimo = get_int(raw, 20) if used >= 22 else 0
    cont_v = int(get_int(raw, 22)) if used >= 24 else 0
    peso_v = round(float(get_real(raw, 24)), 4) if used >= 28 else 0.0
    return {
        "materiales": {
            "plastico": {"piezas": int(get_int(raw, 0)), "pesoKg": round(float(get_real(raw, 4)), 4)},
            "aluminio": {"piezas": int(get_int(raw, 2)), "pesoKg": round(float(get_real(raw, 8)), 4)},
            "vidrio": {"piezas": cont_v, "pesoKg": peso_v},
        },
        "finalizada": bool(get_bool(raw, 16, 1)) if used >= 17 else False,
        "plc": {
            "conectado": True,
            "sistemaOn": bool(get_bool(raw, 16, 2)) if used >= 17 else False,
            "modoAuto": bool(get_bool(raw, 16, 3)) if used >= 17 else False,
            "emergencia": bool(get_bool(raw, 16, 4)) if used >= 17 else False,
            "alarma": bool(get_bool(raw, 16, 5)) if used >= 17 else False,
            "banda": bool(get_bool(raw, 16, 6)) if used >= 17 else False,
            "piston": bool(get_bool(raw, 16, 7)) if used >= 17 else False,
            # P1 plástico · P2 latas · P3 vidrio
            "piston1": bool(get_bool(raw, 17, 0)) if used >= 18 else False,
            "piston2": bool(get_bool(raw, 17, 1)) if used >= 18 else False,
            "piston3": bool(get_bool(raw, 17, 2)) if used >= 18 else False,
            "estado": ESTADO_TXT.get(estado, "idle"),
            "ultimoMaterial": MATERIAL_TXT.get(ultimo),
            "pesoActualKg": round(float(get_real(raw, 12)), 4) if used >= 16 else 0.0,
            "sesionActiva": bool(get_bool(raw, 16, 0)) if used >= 17 else False,
        },
    }


def escribir_db_hmi(client: snap7.client.Client, db_hmi: int, cmd: dict) -> None:
    peso = cmd.get("PesoActualKg", 0.0)
    try:
        peso = float(peso)
    except (TypeError, ValueError):
        peso = 0.0

    last_err: Exception | None = None
    for sz in DB_HMI_WRITE_FALLBACKS:
        raw = bytearray(sz)
        for key, byte, bit in BOOL_MAP:
            if byte >= sz:
                continue
            set_bool(raw, byte, bit, bool(cmd.get(key, False)))
        if PESO_OFFSET + 4 <= sz:
            set_real(raw, PESO_OFFSET, peso)
        try:
            client.db_write(db_hmi, 0, raw)
            if sz < DB_HMI_WRITE_SIZE:
                _warn_once(
                    "hmi_parcial",
                    f"⚠️  DB_HMI (DB{db_hmi}) solo admite escritura de {sz} B "
                    f"(ideal write {DB_HMI_WRITE_SIZE}, DB ≥ {DB_HMI_SIZE}). "
                    "En TIA: ManualPiston1/2 @1.4/1.5 · SensorVidrio @1.6 · "
                    "PesoActualKg @2.0 · VisionMaterial Int @6.0 → Download. "
                    f"Mientras tanto se escriben {sz} bytes. "
                    "Ver plc_real/FIX_DB_INVALID_ADDRESS.md",
                )
            return
        except Exception as e:
            last_err = e
            continue
    raise RuntimeError(
        f"No se pudo escribir DB_HMI (DB{db_hmi}). "
        "Invalid address / Optimized ON / DB inexistente en esta IP. "
        "Ver plc_real/FIX_DB_INVALID_ADDRESS.md"
    ) from last_err


def reset_sesion_en_plc(client: snap7.client.Client, db_number: int) -> None:
    from snap7.util import set_int

    raw = None
    for sz in DB_READ_FALLBACKS:
        try:
            raw = bytearray(client.db_read(db_number, 0, sz))
            break
        except Exception:
            continue
    if raw is None:
        raise RuntimeError(f"No se pudo leer DatosEstacion DB{db_number} para reset")

    set_int(raw, 0, 0)
    set_int(raw, 2, 0)
    if len(raw) >= 8:
        set_real(raw, 4, 0.0)
    if len(raw) >= 12:
        set_real(raw, 8, 0.0)
    if len(raw) >= 16:
        set_real(raw, 12, 0.0)
    if len(raw) >= 17:
        set_bool(raw, 16, 0, True)
        set_bool(raw, 16, 1, False)
    if len(raw) >= 22:
        set_int(raw, 20, 0)
    if len(raw) >= 24:
        set_int(raw, 22, 0)
    if len(raw) >= 28:
        set_real(raw, 24, 0.0)
    client.db_write(db_number, 0, raw)
    print("↺ DatosEstacion reiniciado.")


def main() -> None:
    args = parse_args()
    print(f"PLC {args.ip} r{args.rack}s{args.slot} | DatosEstacion=DB{args.db} | DB_HMI=DB{args.db_hmi}")

    try:
        plc = conectar_plc(args.ip, args.rack, args.slot)
    except Exception as e:
        print("❌ snap7 no conectó. ¿PLCSIM Advanced RUN? ¿IP de la instancia correcta?")
        print(f"   {e}")
        sys.exit(1)
    print("✅ PLC conectado.")
    diagnostico_dbs(plc, args.db, args.db_hmi)

    if args.reset_on_start:
        try:
            reset_sesion_en_plc(plc, args.db)
        except Exception as e:
            print(f"⚠️  reset: {e}")

    cmd_ref = None
    sesion_ref = None
    if not args.dry_run:
        cred = credentials.Certificate(SERVICE_ACCOUNT_PATH)
        firebase_admin.initialize_app(cred)
        fs = firestore.client()
        sesion_ref = fs.collection("sesiones_activas").document(args.estacion_id)
        cmd_ref = fs.collection("hmi_comandos").document(args.estacion_id)
        if not sesion_ref.get().exists:
            sesion_ref.set({
                "materiales": {
                    "plastico": {"piezas": 0, "pesoKg": 0.0},
                    "aluminio": {"piezas": 0, "pesoKg": 0.0},
                },
                "finalizada": False,
                "plc": {"conectado": False},
                "actualizado": firestore.SERVER_TIMESTAMP,
            })
            print(f"📄 Creado sesiones_activas/{args.estacion_id}")
        print(f"✅ Firestore: sesiones_activas + hmi_comandos / {args.estacion_id}")
    else:
        print("dry-run: sin Firebase")

    print("Loop… Ctrl+C sale.\n")
    try:
        while True:
            # 1) HMI web → PLC
            if not args.dry_run and not args.no_hmi_write and cmd_ref is not None:
                try:
                    snap = cmd_ref.get()
                    if snap.exists:
                        cmd = snap.to_dict() or {}
                        cmd_plc = aplicar_retencion_pulsos(cmd, args.pulse_hold)
                        escribir_db_hmi(plc, args.db_hmi, cmd_plc)
                        # Diagnóstico pulsos Start/Stop
                        if any(cmd_plc.get(k) for k in ("Start", "Stop", "ResetAlarma", "FinSesion")):
                            print(
                                f"   HMI→DB3 pulse "
                                f"Start={int(bool(cmd_plc.get('Start')))} "
                                f"Stop={int(bool(cmd_plc.get('Stop')))} "
                                f"Reset={int(bool(cmd_plc.get('ResetAlarma')))} "
                                f"Fin={int(bool(cmd_plc.get('FinSesion')))}"
                            )
                        # Diagnóstico pistones manuales (P1/P2/P3)
                        mp1 = bool(cmd.get("ManualPiston1"))
                        mp2 = bool(cmd.get("ManualPiston2"))
                        mp3 = bool(cmd.get("ManualPiston"))
                        if mp1 or mp2 or mp3 or not bool(cmd.get("ModoAuto", True)):
                            print(
                                f"   HMI→DB3 Manual P1={int(mp1)} P2={int(mp2)} P3={int(mp3)} "
                                f"Auto={int(bool(cmd.get('ModoAuto')))} "
                                f"BandaMan={int(bool(cmd.get('ManualBanda')))}"
                            )
                except Exception as e:
                    _warn_once("write_hmi_err", f"⚠️  escritura DB_HMI: {e}")

            # 2) PLC → web
            try:
                payload = leer_datos_estacion(plc, args.db)
            except Exception as e:
                _warn_once(
                    "read_datos_err",
                    f"⚠️  lectura DatosEstacion: {e}\n"
                    "   Tip: py plc_probe.py --ip <misma_IP>  |  plc_real/FIX_DB_INVALID_ADDRESS.md",
                )
                time.sleep(1.0)
                continue

            m = payload["materiales"]
            st = payload["plc"]
            print(
                f"→ P:{m['plastico']['piezas']}/{m['plastico']['pesoKg']:.3f} "
                f"A:{m['aluminio']['piezas']}/{m['aluminio']['pesoKg']:.3f} "
                f"{st['estado']} B={int(st['banda'])} P={int(st['piston'])}"
                + (" FIN" if payload["finalizada"] else "")
            )

            if not args.dry_run and sesion_ref is not None:
                sesion_ref.set(
                    {
                        "materiales": payload["materiales"],
                        "finalizada": payload["finalizada"],
                        "plc": payload["plc"],
                        "actualizado": firestore.SERVER_TIMESTAMP,
                    },
                    merge=True,
                )

            if payload["finalizada"]:
                print("\n✅ FinSesion en PLC.")
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nStop.")
    finally:
        try:
            plc.disconnect()
        except Exception:
            pass


if __name__ == "__main__":
    main()
