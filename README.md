# Guacamayos — Estación de clasificación + app (Siemens Youth Innovation Search 2026)
py plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
App web + puente al PLC Siemens (TIA Portal / PLCSIM) + guías para Automation Studio.

## Empieza aquí (stack actual)

**Rúbrica:** Automation Studio + TIA + físico = **obligatorio**. Web Guacamayos = **plus / innovación**.  
Mapa: [`docs/12_RUBRICA_CAPAS.md`](docs/12_RUBRICA_CAPAS.md) · arquitectura: [`docs/10_ARQUITECTURA_FINAL.md`](docs/10_ARQUITECTURA_FINAL.md).

TIA V20 · CPU **1511C-1 PN** (demo AS/KEP) · PLCSIM Advanced **V7** · AS 10 · KEP 6.  
PLC real: CPU **1215C** (`6ES7 215-1HG40-0XB0` V4.5) → carpeta [`plc_real/`](plc_real/).  
HMI usable + app = web (plus); atajo lab sin AS: [`docs/11_SIN_AS_SOLO_WEB.md`](docs/11_SIN_AS_SOLO_WEB.md).

1. [`docs/12_RUBRICA_CAPAS.md`](docs/12_RUBRICA_CAPAS.md) — **qué es obligatorio vs plus**  
2. [`docs/14_GUIA_TIA_PLC_DESDE_CERO.md`](docs/14_GUIA_TIA_PLC_DESDE_CERO.md) — **TIA + PLC desde cero (paso a paso)**  
3. [`docs/13_CONECTAR_TODO.md`](docs/13_CONECTAR_TODO.md) — **cómo conectar AS + TIA + físico + web**  
4. [`docs/02_GUIA_AUTOMATION_STUDIO.md`](docs/02_GUIA_AUTOMATION_STUDIO.md) — AS (rúbrica)  
5. [`tia/TABLA_TAGS_DESDE_CERO.md`](tia/TABLA_TAGS_DESDE_CERO.md) — tags (3 pistones)  
6. [`tia/MAPA_DB_HMI.md`](tia/MAPA_DB_HMI.md) — DB comandos + sensores  
7. [`tia/MAPA_IO_Y_DB.md`](tia/MAPA_IO_Y_DB.md) — DatosEstacion  
8. [`docs/GUION_DEFENSA_15MIN.md`](docs/GUION_DEFENSA_15MIN.md) — defensa 15 min  
9. Resto en [`docs/`](docs/)
### Bridge con PLCSIM Advanced (demo)

```bash
pip install firebase-admin python-snap7
python plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
```

En la app: conectar estación → **Abrir HMI** · **🔌** = panel PLC real · **🏠** = estaciones.

### PLC real (S7-1200 1215C DC/DC/DC) — carpeta aparte

**No uses el proyecto TIA del 1511C.** Todo está en [`plc_real/`](plc_real/README.md):

```bash
python plc_real/plc_bridge_real.py --ip 192.168.0.10
```

Estación Firestore: `colegio-don-bosco-real`.  
Hardware: **3 pistones doble efecto** — 6 solenoides Ext/Ret · P1 plástico · P2 latas · P3 vidrio.  
Semáforo vía relés (`1L`/`2L` = 24 V). HMI de mesa: **web** (plus) — ver `plc_real/TABLA_IO_1214C.md`. Rúbrica: AS + TIA + este físico.

### Visión Gemini (Raspberry Pi 4) — Costa Rica

Los 3 sensores físicos son **posición** por vía. Gemini identifica el material → `DB_HMI.VisionMaterial`.  
El PLC **solo para la banda** cuando `VisionMaterial==N` **y** el sensor de esa vía se activa.

Ver [`costa_rica/`](costa_rica/) · ladder [`plc_real/NETWORKS_LAD.md`](plc_real/NETWORKS_LAD.md) · qué falta: [`costa_rica/QUE_NECESITO_GEMINI.md`](costa_rica/QUE_NECESITO_GEMINI.md).

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
