#!/usr/bin/env python3
"""
SIBU — Bridge PLC REAL (S7-1200 CPU 1215C DC/DC/DC)

Hardware: 6ES7 215-1HG40-0XB0 · FW V4.5 (ver plc_real/CPU.md)

Mismos DBs que el demo (DatosEstacion=DB1, DB_HMI=DB3), distinta estación Firestore
y distinta IP (la del 1215C en la red, no PLCSIM).

  py plc_real/plc_bridge_real.py
  py plc_real/plc_bridge_real.py --ip 192.168.0.1

Requiere serviceAccountKey.json en la raíz del repo.
Docs: plc_real/README.md
"""

from __future__ import annotations

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
os.chdir(ROOT)

DEFAULT_ESTACION = "colegio-don-bosco-real"
DEFAULT_IP = "192.168.0.1"


def _inject_defaults(argv: list[str]) -> list[str]:
    """Si no pasas estación/IP, usa defaults del PLC real (perfil mesa)."""
    args = list(argv)
    # py script.py  →  py script.py colegio-don-bosco-real --ip ... --perfil mesa
    if len(args) == 1:
        return [
            args[0],
            DEFAULT_ESTACION,
            "--ip",
            DEFAULT_IP,
            "--db",
            "1",
            "--db-hmi",
            "3",
            "--perfil",
            "mesa",
        ]
    # py script.py --ip x  → inserta estación
    if len(args) > 1 and args[1].startswith("-"):
        args = [args[0], DEFAULT_ESTACION] + args[1:]
    # Asegurar perfil mesa si no lo pasaron
    if "--perfil" not in args:
        args += ["--perfil", "mesa"]
    return args


if __name__ == "__main__":
    sys.argv = _inject_defaults(sys.argv)
    print("=== SIBU bridge · PLC REAL 1215C (6ES7 215-1HG40-0XB0 V4.5) · perfil MESA ===")
    print(f"CWD={ROOT}")
    print(f"Args: {' '.join(sys.argv[1:])}")
    print("DB_HMI: Manual_Banda @ DBX1.3 (ver plc_real/DB_HMI_MESA.md)\n")
    from plc_bridge import main

    main()
