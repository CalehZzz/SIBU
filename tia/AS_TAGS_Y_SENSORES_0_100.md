# Automation Studio — tags equivalentes (**legado AS**)

> **Proyecto actual (web / PLC real):** pistones **sin** sensores 0 % / 100 %.  
> Usa `tia/NETWORKS_WEB_ONLY.md` / `plc_real/NETWORKS_LAD.md` (ciclo por TON).  
> Este archivo solo aplica si aún usas Automation Studio con detectores de cilindro.

Nombres actuales (iguales en TIA, KEPServer y AS):

| En TIA / KEP | Dirección | Qué es en Automation Studio |
|---|---|---|
| `M_SensorPieza` | `%M2.0` | Pulsador o detector “hay pieza” |
| `M_SensorPlastico` | `%M2.1` | Pulsador / sensor material plástico |
| `M_SensorAluminio` | `%M2.2` | Pulsador / sensor material aluminio |
| `M_BasculaLista` | `%M2.3` | Señal “báscula lista” |
| ~~`M_PistonRetractado`~~ | ~~`%M2.4`~~ | **No usar** en el diseño actual (sin FC) |
| ~~`M_PistonExtendido`~~ | ~~`%M2.5`~~ | **No usar** en el diseño actual (sin FC) |
| `M_Banda` | `%M3.0` | Bobina / motor / lámpara de la banda |
| `M_Piston` | `%M3.1` | Solenoide (legado 1 pistón) / ver Ext/Ret en docs actuales |
| `M_LamparaRun` | `%M3.2` | Lámpara verde (opcional) |
| `M_LamparaAlarma` | `%M3.3` | Lámpara alarma (opcional) |
| `M_LamparaEmergencia` | `%M3.4` | Lámpara emergencia (opcional) |

**No pongas en AS:** Start, Stop, Emergencia, `DB_HMI`, `DatosEstacion` → eso va por la web.

---

## Idea clave del pistón (por qué hay DOS tags)

Imagina el cilindro como una regla de 0 a 100:

```
  [======== cilindro ========]→ vástago
   ↑                          ↑
  0%                        100%
  “retractado”              “extendido”
  M_PistonRetractado        M_PistonExtendido
```

No es **un** sensor con dos tags.  
Son **dos sensores distintos**, cada uno en una posición:

| Posición en el cilindro | Cuándo se activa | Tag |
|---|---|---|
| **0%** | Vástago **adentro** (casa) | `M_PistonRetractado` |
| **100%** | Vástago **afuera** (empujó aluminio) | `M_PistonExtendido` |

La “Referencia de sensor unidimensional” en AS es solo el **punto magnético** en el cuerpo del cilindro. Cuando el pistón llega ahí, ese contacto pasa a 1.

---

## Paso a paso: poner 0% y 100% en AS

### 1) Armar neumática mínima
- Fuente de presión
- Cilindro de **doble efecto**
- Válvula **5/2** (solenoide + retorno por muelle)
- 2 escapes

### 2) Colocar DOS referencias unidimensionales
1. Busca en la librería: **Referencia de sensor unidimensional** (o “Sensor Ref.” / “1D sensor ref.”)
2. Arrastra **la primera** sobre el cilindro
3. En propiedades de esa ref., busca **Posición** / **Position** / **%** → pon **0** (o 0%)
4. Arrastra **la segunda** (otro componente, no el mismo)
5. Posición → **100** (o 100%)

Quedan **dos símbolos** sobre el cilindro, uno cerca de la base (0%) y otro cerca del final de carrera (100%).

> Si solo puedes soltar una ref. y luego “duplicar”: duplica y cambia la posición de la copia a 100%.

### 3) Enlazar cada una a su tag (lo importante)

Cada ref. necesita su propia **Asignación de variables** / enlace OPC:

#### Sensor @ 0%
1. Doble clic en la ref. que está en **0%**
2. Entra a **Asignación de variables** (*Variable Assignment*)
3. Elige / crea la variable OPC: **`M_PistonRetractado`**
4. Aceptar

#### Sensor @ 100%
1. Doble clic en la ref. que está en **100%** (la otra)
2. **Asignación de variables**
3. Variable: **`M_PistonExtendido`**
4. Aceptar

Así queda:

```
Ref @ 0%   →  M_PistonRetractado  →  KEP %M2.4  →  TIA
Ref @ 100% →  M_PistonExtendido   →  KEP %M2.5  →  TIA
```

### 4) Si la ref. no tiene “Asignación de variables”
En muchas librerías educativas el flujo es:

1. Pones en el esquema eléctrico un **Detector de proximidad NA** (o relé sensor) por cada posición  
2. Doble clic en la **ref. del cilindro** → Asignación de variables → enlazas esa ref. al detector  
3. El **tag OPC** se lo pones al detector (o al enlace OPC del detector):
   - Detector del 0% → `M_PistonRetractado`
   - Detector del 100% → `M_PistonExtendido`

El nombre no va “dentro del 0%/100%” como texto mágico: va en el **enlace de variable** de cada componente.

### 5) Solenoide del pistón
1. Doble clic en la **válvula 5/2**
2. **Asignación de variables** → solenoide `?(ls)` o similar
3. Variable: **`M_Piston`** (`%M3.1`)

Cuando el PLC pone `M_Piston = 1`, la válvula conmuta y el cilindro sale.  
Al llegar a 100%, se activa solo `M_PistonExtendido`.

---

## Cómo comprobar que quedó bien (sin web)

1. KEPServer conectado a PLCSIM  
2. AS en simulación  
3. En TIA **online**, mira `%M2.4` y `%M2.5`:
   - Cilindro adentro → `M_PistonRetractado = 1`, `M_PistonExtendido = 0`
   - Fuerza `M_Piston = 1` (o START + secuencia aluminio) → cilindro sale → al final `M_PistonExtendido = 1`

---

## Errores típicos

| Error | Qué pasa |
|---|---|
| Solo una ref. en el cilindro | Solo detectas una posición |
| Las dos refs. con el **mismo** tag | TIA ve siempre el mismo bit |
| Poner tag `M_Piston` en un sensor | `M_Piston` es la **salida** (solenoide), no el sensor |
| Usar sensor **bidimensional** | No sirve para un vástago lineal |
| Nombres viejos `I_Piston…` | Cámbialos a `M_PistonRetractado` / `M_PistonExtendido` |

---

## Mapa mental rápido

```
WEB 🖥️  →  DB_HMI  →  START / STOP / modo
AS      →  M_Sensor* / M_PistonRetractado / M_PistonExtendido  (entradas proceso)
PLC     →  M_Banda / M_Piston                               (salidas a AS)
PLC     →  DatosEstacion                                    (estado a la web)
```
