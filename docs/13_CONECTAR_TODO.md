# Cómo conectar TODO junto

Dos demos, **mismo cerebro Siemens**. No mezcles IPs ni proyectos TIA.

```
┌─────────────────────────────────────────────────────────────┐
│  DEMO A — Software (rúbrica AS + TIA + HMI)                 │
│                                                             │
│  Automation Studio 10                                       │
│        ↕ tags %M                                            │
│  KEPServerEX 6                                              │
│        ↕ Siemens TCP                                        │
│  TIA + PLCSIM Advanced (CPU 1511C)                          │
│        ↕ snap7                                              │
│  plc_bridge.py  →  Firestore  →  index.html (plus)          │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  DEMO B — Físico (rúbrica campo + plus CR)                  │
│                                                             │
│  Mesa: banda + 3 pistones + sensores + contadores           │
│        ↕ I/Q cableados                                      │
│  CPU 1214C (proyecto TIA aparte: plc_real/)                 │
│        ↕ snap7 (LAN)                                        │
│  Raspberry Pi 4                                             │
│    ├─ plc_bridge_real.py  → Firestore → web                 │
│    ├─ vision Gemini       → DB_HMI.VisionMaterial           │
│    ├─ RFID Arduino USB    → unlock por cuenta               │
│    └─ ESP32 bins WiFi     → bins_pi/estado                  │
└─────────────────────────────────────────────────────────────┘
```

**Regla:** AS/KEP solo en Demo A. En Demo B el “Automation Studio” ya fue la sim; en mesa mandan I/Q reales.

---

## DEMO A — Encender en este orden (PC de Carla)

### 1) TIA / PLCSIM
1. Abre proyecto **1511C** (sim, no el `plc_real`).
2. PLCSIM Advanced → instancia RUN · IP ej. `192.168.0.1`.
3. PUT/GET ON · DBs Optimized OFF (`DatosEstacion` DB1, `DB_HMI` DB3).

### 2) KEPServerEX 6
1. Canal Siemens TCP → IP de PLCSIM.
2. Tags `%M` según `tia/KEPSERVER_AS_MAPEO.md` (sensores AS→PLC, actuadores PLC→AS).
3. **No** mapees `DB_HMI` ni `DatosEstacion` en KEP.

### 3) Automation Studio 10
1. Circuito: aire → 5/2 → cilindro doble efecto → sensores 0% / 100%.
2. Alias = mismos nombres que KEP (`M_Piston`, `M_SensorPlastico`, …).
3. Simulación ON. El PLC manda solenoide; AS mueve; sensor vuelve.

### 4) Bridge + web (plus)
```bash
# En la PC (serviceAccountKey.json en la raíz del repo)
pip install firebase-admin python-snap7
python plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
```
Abre `index.html` / Hosting → Abrir HMI → Start Auto.

### Prueba de fuego A
Start → ciclo plástico en AS → ciclo aluminio → contador en web.  
Si AS no se mueve: KEP tags. Si web no: bridge/IP. Si PLC no avanza: LAD.

Guías: `docs/02_GUIA_AUTOMATION_STUDIO.md` · `tia/KEPSERVER_AS_MAPEO.md` · `docs/03_CONEXION_WEB_PLC.md`

---

## DEMO B — Encender en este orden (mesa Costa Rica)

Proyecto TIA **distinto**: `plc_real/` · CPU **1214C**.

### 1) Cableado + PLC
1. I/Q según `plc_real/TABLA_IO_1214C.md`.
2. Descarga LAD (`NETWORKS_LAD.md`) · RUN.
3. Red: PLC `192.168.0.10` (o la tuya) · Pi `192.168.0.20` misma subnet.
4. PUT/GET ON · mismos DBs Optimized OFF.

### 2) Raspberry Pi
```bash
cd ~/SIBU
git pull
# costa_rica/sibu.env → GEMINI_API_KEY, PLC_IP, RFID_SERIAL, BINS_PORT
bash costa_rica/install_services.sh all
```
Eso levanta: visión Firestore + RFID + bins + bridge→PLC.

Solo web/visión/RFID/bins (sin PLC aún):
```bash
bash costa_rica/install_services.sh web
```

### 3) Periféricos
| Pieza | Cable | Destino |
|---|---|---|
| Arduino RC522 | USB → Pi (`/dev/ttyACM0` o `USB0`) | `sibu-rfid-gate` |
| ESP32 (HC + LCD + HX711) | WiFi → Pi `:8082` | `sibu-bins` |
| Cámara | USB Pi | `vision_gemini` |
| Contadores / solenoides / banda | a 1214C | I/Q reales |

### 4) Web (mismo plus)
App SIBU (Hosting) → login → estación `colegio-don-bosco-real` → HMI / 📷 / RFID.

### Prueba de fuego B
RFID unlock → Start web → pieza → Gemini material → sensor vía → pistón → contador físico + web.  
Bins: nivel en panel. Peso HX711 solo al final (demo).

---

## Qué habla con qué (contrato único)

| Quién | Canal | Qué mueve |
|---|---|---|
| AS ↔ PLC (sim) | KEP → `%M` | Sensores / banda / pistón simulados |
| Web ↔ PLC | Firestore ↔ `plc_bridge` ↔ snap7 | `DB_HMI` (comandos) + `DatosEstacion` (estado) |
| Vision ↔ PLC | Pi escribe `DB_HMI.VisionMaterial` | Material 0–4 |
| RFID ↔ web | Firestore `rfid_gate` | Unlock por cuenta Google |
| Bins ↔ web | Firestore `bins_pi/estado` | % llenado / peso |

El **PLC siempre decide** actuadores. La web solo escribe comandos en DB. AS solo simula neumática en Demo A.

---

## Defensa 15 min — qué mostrar cuándo

| Min | Conectado | Qué ve el jurado |
|---:|---|---|
| Carla AS+TIA | Demo A corriendo | Cilindro + LAD |
| Caleb HMI | Demo A + bridge | Clicks Start / contadores |
| Demo integral | Demo A | Los tres alineados |
| Cierre físico | Demo B (vivo o video) | Mesa real |

No hace falta AS y mesa al mismo tiempo. Son **dos demos del mismo diseño**.

---

## Si algo no conecta

| Síntoma | Revisar |
|---|---|
| AS no mueve cilindro | KEP quality, alias `%M`, PLCSIM RUN |
| Web Start no hace nada | bridge corriendo, DB3 offsets, PUT/GET |
| Mesa pistón no sale | I/Q tabla, VisionMaterial + sensor vía, aire |
| RFID silencioso en web | `RFID_SERIAL`, un solo proceso en el puerto, `journalctl -u sibu-rfid-gate` |
| ESP bins offline | WiFi Pi IP, POST `:8082`, `sibu-bins` activo |

Checklist PLC real: `plc_real/CHECKLIST.md`  
Rúbrica capas: `docs/12_RUBRICA_CAPAS.md`
