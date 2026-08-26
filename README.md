# Guacamayos — Estación de clasificación + app (Siemens Youth Innovation Search 2026)
py plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
App web + puente al PLC Siemens (TIA Portal / PLCSIM) + guías para Automation Studio.

## Empieza aquí (stack actual)

**Lee primero:** [`docs/10_ARQUITECTURA_FINAL.md`](docs/10_ARQUITECTURA_FINAL.md) · sim HMI: [`docs/11_SIN_AS_SOLO_WEB.md`](docs/11_SIN_AS_SOLO_WEB.md)  
TIA V20 · CPU **1511C-1 PN** (demo, **3 pistones sim**) · PLCSIM Advanced **V7** · HMI virtual en la web.  
PLC real: CPU **1214C** → carpeta [`plc_real/`](plc_real/).

1. [`docs/11_SIN_AS_SOLO_WEB.md`](docs/11_SIN_AS_SOLO_WEB.md) — sim solo HMI · 3 pistones  
2. [`tia/TABLA_TAGS_DESDE_CERO.md`](tia/TABLA_TAGS_DESDE_CERO.md) — **todas las tags (3 pistones)**  
3. [`tia/MAPA_DB_HMI.md`](tia/MAPA_DB_HMI.md) — DB de comandos + sensores sim  
4. [`tia/MAPA_IO_Y_DB.md`](tia/MAPA_IO_Y_DB.md) — DatosEstacion (+ Piston1/2/3On)  
5. [`tia/NETWORKS_WEB_ONLY.md`](tia/NETWORKS_WEB_ONLY.md) — networks LAD sim  
6. [`docs/GUION_DEFENSA_15MIN.md`](docs/GUION_DEFENSA_15MIN.md) — guion defensa 15 min  
7. Resto de guías en [`docs/`](docs/)
### Bridge con PLCSIM Advanced (demo)

```bash
pip install firebase-admin python-snap7
python plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
```

En la app: conectar estación → **Abrir HMI** · **🔌** = panel PLC real · **🏠** = estaciones.

### PLC real (S7-1200 1214C) — carpeta aparte

**No uses el proyecto TIA del 1511C.** Todo está en [`plc_real/`](plc_real/README.md):

```bash
python plc_real/plc_bridge_real.py --ip 192.168.0.10
```

Estación Firestore: `colegio-don-bosco-real`.  
Hardware: **3 pistones doble efecto** — 6 solenoides Ext/Ret · P1 plástico · P2 latas · P3 vidrio.  
Semáforo vía relés (`1L`/`2L` = 24 V). Operador: **solo web** — ver `plc_real/TABLA_IO_1214C.md`.

## App

- `index.html` — usuario en estación (plástico + latas + vidrio) + HMI desde la estación
- `plc_simulador.py` — genera datos sin PLC (para probar la web)
- `plc_bridge.py` — lee el PLC virtual con snap7 y publica a Firestore
- `index.js` — Cloud Function auxiliar para roles admin

## Demo rápida sin TIA

```bash
pip install firebase-admin
# coloca serviceAccountKey.json en la raíz
# abre index.html en un servidor local, pulsa Conectar
python plc_simulador.py parque-central
```

## Demo con PLC virtual

```bash
pip install firebase-admin python-snap7
# PLCSIM RUN + NetToPLCSim Start Server
python plc_bridge.py parque-central --ip 127.0.0.1 --reset-on-start
```
