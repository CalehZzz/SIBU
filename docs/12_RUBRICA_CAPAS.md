# Rúbrica → qué es obligatorio y qué es plus

Siemens Youth Innovation Search 2026 · Guacamayos

## Lectura correcta (no confundir)

| Capa | Qué es | Rúbrica |
|---|---|---|
| **TIA Portal + PLC** | Cerebro: LAD, modos, secuencia, alarmas, contadores | **Obligatorio** (~20 pts programación PLC) |
| **Automation Studio** | Electroneumática: cilindros, 5/2, sensores 0%/100% | **Obligatorio** (~15 pts simulación AS) |
| **Físico (mesa)** | Banda, pistones, sensores, contadores, PLC 1214C | **Obligatorio** para demostrar que baja a campo / funcionamiento integral |
| **HMI** | Start/Stop, auto/manual, estado, alarmas | **Obligatorio** (~15 pts) — puede ser WinCC **o** HMI web |
| **Página web Guacamayos** | App usuario + HMI virtual + Firestore + bridge | **Plus / innovación** (~10 pts innovación + refuerza HMI e integral) |

**Frase para el jurado:**  
> “El núcleo del reto es **Automation Studio + TIA + físico**. La **web es nuestro plus**: misma lógica Siemens, usable en celular y remota.”

## Qué NO decir

- “Todo está en la web; AS es opcional.”
- “Operamos 100 % sin Automation Studio.”
- “El PLC es secundario.”

Esos mensajes chocan con la rúbrica aunque la mesa Costa Rica se mande por Firestore.

## Cómo se demuestran las tres capas

**Paso a paso de conexión:** [`13_CONECTAR_TODO.md`](13_CONECTAR_TODO.md)

```
Demo software (puntos AS + TIA + HMI)
  AS 10  ←→  KEP  ←→  PLCSIM (1511C)  ←→  bridge  ←→  HMI web

Demo física (funcionamiento integral + campo)
  Mesa: banda + 3 pistones + sensores + contadores
  PLC 1214C (I/Q reales)
  Misma web / mismos DBs (plus)
  Visión Gemini + RFID + bins HC  (plus CR)
```

| Momento defensa | Qué mostrar | Criterio |
|---|---|---|
| Carla · AS + KEP | Cilindro 5/2, tags `%M`, feedback 0/100 | Simulación AS |
| Carla · TIA | OB1 / FCs / LAD ONLINE | Programación PLC |
| Caleb · HMI web | Start, ciclo, contadores | HMI + innovación |
| Demo integral | AS + PLC + web alineados | Funcionamiento integral |
| Cierre físico | Mesa real / video | Baja a campo |

## Relación Costa Rica (Pi / Gemini / RFID)

Eso **no sustituye** AS ni el PLC. Es **plus local**:

| Plus CR | Rol |
|---|---|
| Raspberry Pi + Gemini | Identifica material → `VisionMaterial` al PLC |
| RFID por cuenta Google | Acceso / sesión usuario |
| ESP32 HC + LCD + HX711 | Nivel de botes + peso |
| Web Guacamayos | HMI + app (el plus del enunciado) |

El PLC sigue mandando banda y pistones. AS sigue siendo la demo electroneumática del reto. La web no reemplaza ninguna de las dos.

## Checklist mental antes de la defensa

- [ ] AS: circuito con al menos un cilindro doble efecto + 5/2 + sensores
- [ ] TIA: lógica en FCs, no “todo en OB1”
- [ ] Físico: al menos un ciclo visible en mesa (o video claro)
- [ ] Web: presentada como **innovación / plus**, no como único mando del proyecto
- [ ] Una frase explícita: “AS + TIA + físico = rúbrica; web = plus nuestro”
