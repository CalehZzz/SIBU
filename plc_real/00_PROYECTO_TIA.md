# Crear proyecto TIA — CPU 1215C DC/DC/DC (PLC real · 3 pistones)

**Hardware de mesa:** `6ES7 215-1HG40-0XB0` · **CPU 1215C DC/DC/DC** · FW **V4.5**  
(ver `CPU.md`)

El proyecto de **PLCSIM 1511C no sirve** como device de esta CPU.  
Crea un proyecto nuevo con el **MLFB exacto**.

## 1) Proyecto nuevo
1. TIA Portal V20 → Create new project (ej. `SIBU_PLC_Real_1215C`)
2. Add new device → **S7-1200** → **CPU 1215C DC/DC/DC**  
   → MLFB **`6ES7 215-1HG40-0XB0`** (o el de la etiqueta si difiere)
3. Firmware: **V4.5** (el de tu CPU física)

> No elijas 1214C ni AC/DC/Rly: las salidas de esta CPU son **transistor 24 V**, no relé interno.

## 2) Red / IP
1. Device configuration → PROFINET interface [X1]
2. IP estática en la misma red que la Pi (ej. `192.168.0.1` o `192.168.0.10` / mask `255.255.255.0`)
3. Anota esa IP para el bridge (`PLC_IP` en `costa_rica/sibu.env`)

## 3) Protección (obligatorio para snap7)
Device → CPU → **Protection & Security**:
- Permitir acceso con **PUT/GET** desde partners remotos → **ON**
- Access level: Full access (o el mínimo que permita escritura DB)

Download de **hardware** después de cambiar esto.

## 4) Bloques de programa
Crea (mismos nombres que en sim, distinta lógica de I/O):

| Bloque | Lenguaje | Rol |
|---|---|---|
| `OB1` | LAD | Calls |
| `FC_Modos` | LAD | Start/Stop/Emergencia + `M_Clasificando` |
| `FC_Secuencia` | LAD | Banda, **P1/P2/P3**, conteo |
| `FC_Alarmas` | LAD | Alarmas |
| `FC_EspejoWeb` | SCL | Espejo a `DatosEstacion` |

## 5) Data blocks
| DB | Nº | Optimized |
|---|---|---|
| `DatosEstacion` | **1** | **OFF** |
| `DB_HMI` | **3** | **OFF** |

Estructura: `DB_CONTRATO_WEB.md` (incluye `Piston1On`/`Piston2On`/`Piston3On` @ 17.x).

## 6) Tag table
Copia `TABLA_IO_1214C.md` — **6 solenoides** (Ext/Ret) + banda + **3 salidas a relés externos** · DI material / botes · **sin** FC de pistón.  
`%Q` = 24 V DC → bobinas de relé / solenoides 24 V; el 220 V del semáforo solo en contactos de relé.

## 7) Download
1. CPU en STOP o RUN-P según política
2. Download hardware + software
3. RUN

## 8) Bridge
```bash
py plc_real/plc_bridge_real.py --ip <IP_DE_ESTA_CPU>
```

Estación Firestore: `colegio-don-bosco-real`.

| | Sim (1511C) | Real (1215C) |
|---|---|---|
| Device | PLCSIM Advanced | `6ES7 215-1HG40-0XB0` |
| I/O | `%M` / AS / KEP | `%I` / `%Q` físicos |
| Estación web | `parque-central` | `colegio-don-bosco-real` |
