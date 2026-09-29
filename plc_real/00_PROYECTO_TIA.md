# Crear proyecto TIA — CPU 1214C AC/DC/Rly (PLC real · 3 pistones)

El proyecto de **PLCSIM 1511C no sirve** como device del 1214C.  
Crea un proyecto nuevo.

## 1) Proyecto nuevo
1. TIA Portal V20 → Create new project (ej. `SIBU_PLC_Real_1214C`)
2. Add new device → **S7-1200** → **CPU 1214C AC/DC/Rly**  
   (elige el MLFB exacto de tu CPU, ej. `6ES7 214-1BG40-0XB0` o el que tengas)
3. Firmware: el de tu CPU física

## 2) Red / IP
1. Device configuration → PROFINET interface [X1]
2. IP estática en la misma red que tu PC (ej. `192.168.0.10` / mask `255.255.255.0`)
3. Anota esa IP para el bridge

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
Copia `TABLA_IO_1214C.md` — **6 solenoides** (Ext/Ret) + banda + **3 relés** · **5 DI** material · **sin** FC de pistón.  
Cablea **1L y 2L a +24 V**; el 220 V del semáforo solo en contactos de relé.

## 7) Download
1. CPU en STOP o RUN-P según política
2. Download hardware + software
3. RUN
4. Online: verifica DBs y I/Q

## Diferencia clave vs simulación

| | Sim (1511C) | Real (1214C) |
|---|---|---|
| Sensores | `DB_HMI.Sensor*` | **`I_Sensor*`** (pieza / vías) — **sin** FC pistón |
| Peso | simulado en HMI | báscula solo al **final** (demo); **no** espera peso para clasificar |
| Pistones | `M_PistonN` + Ext/Ret · TON | **6 × `Q_PistonNExt/Ret`** · ciclo por tiempo |
| Semáforo | `M_Lampara*` | `Q_Lampara*` → **relés** → 220 V |
| Roles | Tres cilindros sim | P1 plástico · P2 latas · P3 vidrio |
| Operador (Start/Stop/manual…) | `DB_HMI` | **solo `DB_HMI`** (sin pulsadores físicos) |
| AS / KEP | no | no |
