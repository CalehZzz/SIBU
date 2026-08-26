# Mapa de I/O y bloque de datos (DB1)

Copia estos nombres **exactamente** en TIA Portal (PLC tags + DB). Así el bridge Python y la web coinciden.

> **Simulación (solo HMI):** operador + sensores → **`DB_HMI`**.  
> Actuadores → **`M_Banda` / `M_Piston1` (plástico) / `M_Piston2` (latas) / `M_Piston3` (vidrio)**.  
> Guía: `tia/NETWORKS_WEB_ONLY.md` · `docs/11_SIN_AS_SOLO_WEB.md`.

```
  [Sim] → báscula → banda → P1 plástico | P2 latas | P3 vidrio
```

---

## 1. Entradas digitales (`%I`) — opcional / legado

En el modo **web-only** el operador **no** usa estas `%I`. Se dejan documentadas solo si alguien cablea PLCSIM clásico.

| Dirección | Nombre simbólico | Tipo | Descripción |
|---|---|---|---|
| — | *(sin botonera)* | — | Start/Stop/Emergencia/Manual → `DB_HMI` |
| — | Sensores estación | — | `DB_HMI.Sensor*` / `PistonNExtendido` / `PistonNRetractado` |

Para el **PLC real 1214C** ver `plc_real/TABLA_IO_1214C.md` (11 DI proceso + 7 DQ · doble efecto).

---

## 2. Actuadores simulados (`%M`, no `%Q`)

| Nombre | Ejemplo | Descripción |
|---|---|---|
| `M_Banda` | `%M3.0` | Marcha banda |
| `M_Piston1` | `%M3.1` | Empuje plástico |
| `M_Piston2` | `%M3.2` | Empuje latas |
| `M_Piston3` | `%M3.3` | Empuje vidrio |
| `M_LamparaRun` | `%M3.4` | Piloto marcha |
| `M_LamparaAlarma` | `%M3.5` | Piloto alarma |
| `M_LamparaEmergencia` | `%M3.6` | Piloto emergencia |

---

## 3. Memorias internas (`%M`) y temporizadores

| Nombre | Tipo | Ejemplo OK | Uso |
|---|---|---|---|
| `M_SistemaOn` | Bool | `%M0.0` | Latch de sistema energizado |
| `M_ModoAuto` | Bool | `%M0.1` | Copia del modo (DB_HMI) |
| `M_Alarma` | Bool | `%M0.2` | Alarma activa |
| `M_ClasifPlastico` | Bool | `%M0.3` | Secuencia P2 |
| `M_ClasifAluminio` | Bool | `%M0.4` | Secuencia P3 |
| `M_Clasificando` | Bool | `%M0.7` | OR clasif (banda / P1) |
| `T_RetardoPiston2` / `T_RetardoPiston3` | TON IEC | auto | Espera extendido antes de contar |
| `T_TimeoutPiston2` / `T_TimeoutPiston3` | TON IEC | auto | Alarma si no llega a 100% |

> Inserta el bloque **TON** y nombra la instancia (`T_RetardoPiston2`…). Contactos: `T_RetardoPiston2.Q`.  
> Networks: `tia/NETWORKS_WEB_ONLY.md`.
---

## 4. DB1 — `DatosEstacion` (contrato con la web)

Crea un **Data Block** global llamado `DatosEstacion` (número DB1).

### Estructura (optimized = OFF / “Standard” para offsets fijos)

| Offset | Nombre | Tipo | Notas |
|---|---|---|---|
| 0.0 | `ContPlastico` | Int | Piezas plásticas de la sesión |
| 2.0 | `ContAluminio` | Int | Piezas aluminio de la sesión |
| 4.0 | `PesoPlasticoKg` | Real | Peso acumulado plástico (kg) |
| 8.0 | `PesoAluminioKg` | Real | Peso acumulado aluminio (kg) |
| 12.0 | `PesoActualKg` | Real | Último peso leído de la báscula |
| 16.0 | `SesionActiva` | Bool | Hay usuario conectado / sesión abierta |
| 16.1 | `FinSesion` | Bool | PLC pide cerrar sesión (web guarda) |
| 16.2 | `SistemaOn` | Bool | Espejo de marcha |
| 16.3 | `ModoAuto` | Bool | Espejo modo |
| 16.4 | `Emergencia` | Bool | Espejo emergencia |
| 16.5 | `Alarma` | Bool | Espejo alarma |
| 16.6 | `BandaOn` | Bool | Espejo `M_Banda` |
| 16.7 | `PistonOn` | Bool | OR de P1/P2/P3 |
| 17.0 | `Piston1On` | Bool | Plástico |
| 17.1 | `Piston2On` | Bool | Latas |
| 17.2 | `Piston3On` | Bool | Vidrio |
| 18.0 | `EstadoMaquina` | **Int** | 0 idle, 1 running, 2 clasificando, 3 alarma, 4 emergencia |
| 20.0 | `UltimoMaterial` | **Int** | 0 ninguno, 1 plástico, 2 aluminio, 3 vidrio |
| 22.0 | `ContVidrio` | Int | Piezas vidrio |
| 24.0 | `PesoVidrioKg` | Real | kg vidrio |

### Offsets que NO sirven para la web

Si al compilar ves algo como:

- `EstadoMaquina` → **17.0**
- `UltimoMaterial` → **17.1**

eso casi siempre significa que quedaron como **Bool** (bits), no como **Int**.  
El bridge Python espera **Int en 18 y 20**. Corrige el tipo a `Int`, recompila y verifica que digan **18.0** y **20.0**.

También confirma: clic derecho en el DB → Properties → Attributes → **Optimized block access = OFF**.

### Cómo crear el DB en TIA (resumen)

1. Project tree → PLC → Program blocks → Add new block → **Data block**.
2. Name: `DatosEstacion`, Type: Global DB, Language: DB.
3. En Attributes del DB: **desactiva Optimized block access** (importante para snap7).
4. Agrega los campos de la tabla en el mismo orden y con el **tipo exacto** (Int / Real / Bool).
5. Compile y revisa la columna Offset.

---

## 5. Entrada analógica de báscula (opcional / simulado)

Si simulas peso sin hardware real:

- Usa un `Real` en el DB (`PesoActualKg`) escrito desde HMI o desde un FC que genere valores de prueba.
- Si más adelante tienes módulo analógico: `%IW64` → escala a kg.

Rangos típicos de prueba:
- Botella plástica: **0.02 – 0.06 kg**
- Lata aluminio: **0.01 – 0.03 kg**

---

## 6. Lógica de clasificación

| Sensor | Pistón | Contador |
|---|---|---|
| Plástico | P1 | ContPlastico |
| Latas (aluminio) | P2 | ContAluminio |
| Vidrio | P3 | ContVidrio |

---

## 7. Lectura desde Python (offsets del bridge)

```text
ContPlastico      = get_int(db, 0)
ContAluminio      = get_int(db, 2)
PesoPlasticoKg    = get_real(db, 4)
PesoAluminioKg    = get_real(db, 8)
PesoActualKg      = get_real(db, 12)
SesionActiva      = get_bool(db, 16, 0)
FinSesion         = get_bool(db, 16, 1)
SistemaOn         = get_bool(db, 16, 2)
ModoAuto          = get_bool(db, 16, 3)
Emergencia        = get_bool(db, 16, 4)
Alarma            = get_bool(db, 16, 5)
BandaOn           = get_bool(db, 16, 6)
PistonOn          = get_bool(db, 16, 7)
Piston1On         = get_bool(db, 17, 0)
Piston2On         = get_bool(db, 17, 1)
Piston3On         = get_bool(db, 17, 2)
EstadoMaquina     = get_int(db, 18)
UltimoMaterial    = get_int(db, 20)
ContVidrio        = get_int(db, 22)
PesoVidrioKg      = get_real(db, 24)
```

Tamaño mínimo a leer: **28 bytes**.
