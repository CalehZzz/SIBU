# KEPServerEX 6 ↔ Automation Studio 10 — mapeo oficial

> **Proyecto actual:** pistones **sin** FC 0 %/100 %. Ver `tia/NETWORKS_WEB_ONLY.md`.  
> Tags `M_PistonRetractado` / `M_PistonExtendido` abajo son **legado AS**.

**Solo estas tags `%M` van entre AS y el PLC.**  
**No mapees `DB_HMI` ni `DatosEstacion` en KEPServer.**

```
Automation Studio 10  ←→  KEPServerEX 6 (Siemens TCP/IP Ethernet)  ←→  PLC 1511C (PLCSIM Advanced)
```

---

## Resumen por dirección

| Dirección PLC | Tags | Quién escribe | Quién lee |
|---|---|---|---|
| `%M2.0` … `%M2.5` | Sensores proceso | **AS** (simula sensores) | **PLC** (LAD) |
| `%M3.0` … `%M3.1` | Banda, pistón | **PLC** (LAD) | **AS** (actuadores) |
| `%M3.2` … `%M3.4` | Lámparas (opcional) | **PLC** | **AS** (piloto visual) |

---

## Tabla completa — copiar a KEPServer y AS

### Sensores (AS → PLC) — KEPServer: **Read** desde PLC / **Write** desde AS

| Tag PLC | Dirección | Tipo | Variable en Automation Studio | Componente AS |
|---|---|---|---|---|
| `M_SensorPieza` | `%M2.0` | Bool | `M_SensorPieza` | Pulsador / detector “pieza en estación” |
| `M_SensorPlastico` | `%M2.1` | Bool | `M_SensorPlastico` | Pulsador o sensor material plástico |
| `M_SensorAluminio` | `%M2.2` | Bool | `M_SensorAluminio` | Pulsador o sensor material aluminio |
| `M_BasculaLista` | `%M2.3` | Bool | `M_BasculaLista` | Señal “báscula estable / lista” |
| `M_PistonRetractado` | `%M2.4` | Bool | `M_PistonRetractado` | Ref. sensor cilindro @ 0% |
| `M_PistonExtendido` | `%M2.5` | Bool | `M_PistonExtendido` | Ref. sensor cilindro @ 100% |

### Actuadores (PLC → AS) — KEPServer: PLC escribe, AS lee

| Tag PLC | Dirección | Tipo | Variable en Automation Studio | Componente AS |
|---|---|---|---|---|
| `M_Banda` | `%M3.0` | Bool | `M_Banda` | Bobina / relé motor banda (o lámpara demo) |
| `M_Piston` | `%M3.1` | Bool | `M_Piston` | Solenoide válvula 5/2 (asignación variables) |

### Indicadores opcionales (PLC → AS)

| Tag PLC | Dirección | Tipo | En AS |
|---|---|---|---|
| `M_LamparaRun` | `%M3.2` | Bool | Lámpara verde RUN |
| `M_LamparaAlarma` | `%M3.3` | Bool | Lámpara roja alarma |
| `M_LamparaEmergencia` | `%M3.4` | Bool | Lámpara amarilla emergencia |

---

## Qué NO va en KEPServer

| Tag / área | Motivo |
|---|---|
| `DB_HMI.*` | Solo HMI web vía `plc_bridge.py` |
| `DatosEstacion.*` | Solo lectura web vía bridge |
| `M_SistemaOn`, `M_ModoAuto`, `M_Alarma`, `M_Clasificando` | Lógica interna PLC |
| `%I` / `%Q` del 1511C | No se usan en este diseño |

---

## Configuración KEPServerEX 6 (paso a paso)

### 1) Canal
- **Driver:** Siemens TCP/IP Ethernet
- **Modelo:** S7-1500 (o S7-1200/1500 genérico)
- **IP:** la de tu instancia PLCSIM Advanced (ej. `192.168.0.1` o la que asignaste)
- **Rack:** `0`
- **Slot:** `1` (CPU 1511C en rack 0)

### 2) Dispositivo / tags
Crea un tag por fila de la tabla anterior:

| Campo KEP | Valor ejemplo |
|---|---|
| Name | `M_SensorPieza` (mismo nombre que en TIA) |
| Address | `M2.0` o `%M2.0` (según versión KEP) |
| Data type | Boolean / Bit |

Repite para `M2.1` … `M2.5`, `M3.0` … `M3.4`.

### 3) Cliente OPC / enlace AS
- Automation Studio 10 suele enlazar por **OPC DA** o el conector que uses con KEP.
- En AS, en **Asignación de variables** / OPC, asocia cada componente al **mismo nombre** que en KEP (`M_Banda`, `M_SensorPieza`, …).

### 4) Dirección de datos

| Señal | Flujo |
|---|---|
| Sensores `%M2.x` | AS cambia el bit → KEP escribe en PLC → LAD lee `M_Sensor*` |
| Actuadores `%M3.0/1` | LAD escribe `M_Banda`/`M_Piston` → KEP lee PLC → AS mueve banda/solenoide |

---

## Enlace en Automation Studio — por componente

### Válvula 5/2 + cilindro
| AS | Tag OPC / variable | Tag PLC |
|---|---|---|
| Solenoide válvula | `M_Piston` | `%M3.1` |
| Ref. sensor 0% | `M_PistonRetractado` | `%M2.4` |
| Ref. sensor 100% | `M_PistonExtendido` | `%M2.5` |

Configuración válvula: **Especificaciones técnicas** → un lado **Solenoide**, otro **Retorno por muelle**.

### Banda transportadora (o demo)
| AS | Tag | PLC |
|---|---|---|
| Motor / bobina banda | `M_Banda` | `%M3.0` |

### Pulsadores de material (simulación manual en AS)
| AS | Tag | PLC |
|---|---|---|
| Pieza presente | `M_SensorPieza` | `%M2.0` |
| Plástico | `M_SensorPlastico` | `%M2.1` |
| Aluminio | `M_SensorAluminio` | `%M2.2` |
| Báscula lista | `M_BasculaLista` | `%M2.3` |

> Los pulsadores Start/Stop/Emergencia **no** van a AS en la arquitectura final: van por **HMI web** → `DB_HMI` → bridge.

---

## Orden de prueba integrada

1. TIA: Download a PLCSIM Advanced → CPU en **RUN**
2. KEPServer: canal **Connected** (verde)
3. AS: simulación → verifica que al pulsar `M_SensorPieza` en AS, en TIA online `M_SensorPieza` (`%M2.0`) cambia
4. Web: START en panel 🖥️ → en TIA `DB_HMI.Start` y `M_SistemaOn` deben activarse
5. AS: activa sensores plástico → contador `DatosEstacion.ContPlastico` sube (vía bridge en web)

---

## Nombres viejos → nombres actuales

Si en AS aún tienes `I_PistonExtendido` u otros `I_`/`Q_`:

| Nombre viejo (AS/TIA antiguo) | Nombre actual (KEP + TIA) |
|---|---|
| `I_SensorPieza` | `M_SensorPieza` |
| `I_SensorPlastico` | `M_SensorPlastico` |
| `I_SensorAluminio` | `M_SensorAluminio` |
| `I_BasculaLista` | `M_BasculaLista` |
| `I_PistonRetractado` | `M_PistonRetractado` |
| `I_PistonExtendido` | `M_PistonExtendido` |
| `Q_Banda` | `M_Banda` |
| `Q_Piston` | `M_Piston` |

Renómbralos **igual** en TIA tag table, KEPServer y AS para evitar desincronización.

---

## Checklist

- [ ] 10 tags `%M` en TIA (6 sensores + 2 actuadores + 3 lámparas opcionales)
- [ ] Mismos nombres y direcciones en KEPServer
- [ ] AS enlazado por OPC a esos nombres
- [ ] `DB_HMI` y `DatosEstacion` **fuera** de KEP
- [ ] Bridge: `python plc_bridge.py parque-central --ip <IP_PLCSIM> --db 1 --db-hmi 3`
