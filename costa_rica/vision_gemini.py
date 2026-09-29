#!/usr/bin/env python3
"""
SIBU / Guacamayos — Visión en Raspberry Pi 4

Flujo:
  1) Lee flanco de pieza en el PLC (I0.0 vía mirror DB_HMI.SensorPieza o entrada)
  2) Captura foto con la cámara
  3) Llama Gemini → plastico | aluminio | vidrio | desconocido
  4) Escribe DB_HMI.VisionMaterial (Int @ offset 6) = 1 | 2 | 3
  5) El PLC espera el sensor físico de ESA vía para parar banda + pistón
  6) El PLC limpia VisionMaterial:=0 al terminar; aquí no lo pisamos mientras ≠0

Uso:
  export GEMINI_API_KEY=...
  pip install google-generativeai python-snap7 pillow
  python costa_rica/vision_gemini.py --ip 192.168.0.10

Sin API key: arranca en --dry-run de lógica (no llama Gemini).
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time

try:
    import snap7
    from snap7.util import get_bool, get_int, set_int
except ImportError:
    print("Falta python-snap7. pip install python-snap7")
    sys.exit(1)

# Contrato: plc_real/DB_CONTRATO_WEB.md
DB_HMI_DEFAULT = 3
VISION_OFFSET = 6  # Int VisionMaterial
MATERIAL_CODE = {
    "plastico": 1,
    "plastic": 1,
    "aluminio": 2,
    "aluminio/lata": 2,
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


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Pi 4 · Gemini → DB_HMI.VisionMaterial")
    p.add_argument("--ip", default="192.168.0.10", help="IP del PLC")
    p.add_argument("--rack", type=int, default=0)
    p.add_argument("--slot", type=int, default=1)
    p.add_argument("--db-hmi", type=int, default=DB_HMI_DEFAULT)
    p.add_argument("--interval", type=float, default=0.15, help="Poll snap7 (s)")
    p.add_argument(
        "--capture-cmd",
        default="",
        help='Comando que guarda JPEG en --photo-path. Ej: libcamera-still -n -o {path}',
    )
    p.add_argument("--photo-path", default="/tmp/sibu_shot.jpg")
    p.add_argument("--model", default="gemini-3.8-flash")
    p.add_argument(
        "--mock-material",
        default="",
        help="Sin Gemini: fuerza plastico|aluminio|vidrio (prueba ladder)",
    )
    p.add_argument("--dry-run", action="store_true", help="No escribe al PLC")
    return p.parse_args()


def conectar(ip: str, rack: int, slot: int) -> snap7.client.Client:
    client = snap7.client.Client()
    try:
        client.set_connection_type(3)
    except Exception:
        pass
    client.connect(ip, rack, slot)
    if not client.get_connected():
        raise RuntimeError(f"No conectó a {ip}")
    return client


def leer_vision(client: snap7.client.Client, db: int) -> int:
    raw = client.db_read(db, VISION_OFFSET, 2)
    return int(get_int(raw, 0))


def escribir_vision(client: snap7.client.Client, db: int, codigo: int) -> None:
    raw = bytearray(2)
    set_int(raw, 0, int(codigo))
    client.db_write(db, VISION_OFFSET, raw)


def leer_sensor_pieza(client: snap7.client.Client, db: int) -> bool:
    """Preferir I0.0 físico; fallback DB_HMI.SensorPieza @ 1.1 (sim/HMI)."""
    try:
        from snap7.type import Areas

        pe = client.read_area(Areas.PE, 0, 0, 1)
        return bool(pe[0] & 0x01)  # %I0.0
    except Exception:
        raw = client.db_read(db, 0, 2)
        return bool(get_bool(raw, 1, 1))


def capturar(path: str, capture_cmd: str) -> None:
    if capture_cmd:
        cmd = capture_cmd.format(path=path)
        subprocess.run(cmd, shell=True, check=True)
        return
    # Fallback USB OpenCV si está instalado
    try:
        import cv2  # type: ignore
    except ImportError as e:
        raise RuntimeError(
            "Definí --capture-cmd (libcamera-still …) o instalá opencv-python"
        ) from e
    cam = cv2.VideoCapture(0)
    ok, frame = cam.read()
    cam.release()
    if not ok:
        raise RuntimeError("No se pudo leer /dev/video0")
    cv2.imwrite(path, frame)


def clasificar_gemini(path: str, model_name: str, api_key: str) -> str:
    import google.generativeai as genai
    from PIL import Image

    genai.configure(api_key=api_key)
    model = genai.GenerativeModel(model_name)
    img = Image.open(path)
    resp = model.generate_content([PROMPT, img])
    text = (resp.text or "").strip().lower().split()[0]
    text = text.strip(".,;:!?\"'")
    return text


def codigo_de_etiqueta(label: str) -> int:
    """1/2/3 material · 4 desconocido · 0 sin mapear (tratar como 4)."""
    if not label:
        return 4
    code = int(MATERIAL_CODE.get(label, 0))
    return code if code else 4


def main() -> None:
    args = parse_args()
    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    mock = args.mock_material.strip().lower()

    if not api_key and not mock:
        print(
            "⚠️  Sin GEMINI_API_KEY. Exportala o usá --mock-material plastico|aluminio|vidrio.\n"
            "    Ver costa_rica/QUE_NECESITO_GEMINI.md"
        )
        sys.exit(2)

    print(f"Visión → PLC {args.ip} DB{args.db_hmi}.VisionMaterial@{VISION_OFFSET}")
    if mock:
        print(f"Modo mock: siempre '{mock}'")
    elif not api_key:
        print("Sin key (no debería pasar)")
    else:
        print(f"Gemini model={args.model}")

    plc = None
    if not args.dry_run:
        try:
            plc = conectar(args.ip, args.rack, args.slot)
            print("✅ PLC conectado")
        except Exception as e:
            print(f"❌ snap7: {e}")
            sys.exit(1)

    prev_pieza = False
    print("Esperando flanco I0.0 (I_SensorPieza)…")

    while True:
        try:
            if plc is None:
                time.sleep(args.interval)
                continue

            vision = leer_vision(plc, args.db_hmi)
            pieza = leer_sensor_pieza(plc, args.db_hmi)
            rising = pieza and not prev_pieza
            prev_pieza = pieza

            # No pisar mientras el PLC aún no limpió la clasificación anterior
            if vision != 0:
                time.sleep(args.interval)
                continue

            if not rising:
                time.sleep(args.interval)
                continue

            print("📸 Pieza detectada → captura")
            capturar(args.photo_path, args.capture_cmd)

            if mock:
                label = mock
            else:
                label = clasificar_gemini(args.photo_path, args.model, api_key)

            code = codigo_de_etiqueta(label)
            print(f"   Gemini/mock → '{label}' → VisionMaterial={code}")

            if args.dry_run:
                print("   dry-run: no escribo PLC")
            else:
                escribir_vision(plc, args.db_hmi, code)
                print("   ✅ escrito en PLC (1/2/3 clasifica · 4 pass-through)")

        except KeyboardInterrupt:
            print("\nBye")
            break
        except Exception as e:
            print(f"⚠️  {e}")
            time.sleep(1.0)


if __name__ == "__main__":
    main()
