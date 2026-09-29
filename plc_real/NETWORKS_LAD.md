# Networks LAD — PLC real · Gemini + báscula + 3 sensores (IA manda)

## Lógica física (confirmada)

```
[Entrada pieza] → PARA banda → báscula + foto Gemini
       │
       ▼  VisionMaterial listo (1/2/3/4) + peso OK
[Banda sigue]
       │
       ├─ Vision=1 (plástico) → espera SOLO I_SensorPlastico ("detecta todo", al final)
       │                         → PARA → P1 → sigue
       ├─ Vision=2 (lata)     → espera SOLO I_SensorAluminio (metal)
       │                         → PARA → P2 → sigue
       ├─ Vision=3 (vidrio)   → espera SOLO I_SensorVidrio
       │                         → PARA → P3 → sigue
       └─ Vision=4 (desconocido) → NO para por clasificación; deja pasar al final
                                    y limpia VisionMaterial
```

**Reglas:**
1. Gemini decide el material (`VisionMaterial`). Los sensores **no clasifican**.
2. Pueden activarse varios sensores (el de plástico “detecta todo”, etc.): **se ignoran** si no son el de la vía pedida por la IA.
3. La banda **solo** para por: (a) pesar+foto, (b) sensor de la vía del material detectado, (c) empuje de pistón, (d) stop/alarma/emergencia.
4. `desconocido` (4) o timeout → **dejar pasar**, sin pistón.

| Pistón | Ext/Ret | Sensor físico | Qué detecta el hardware | VisionMaterial |
|---|---|---|---|---|
| P1 plástico | Ext/Ret | `I_SensorPlastico` | Óptico “detecta todo” (al **final** de las 3 vías) | **1** |
| P2 latas | Ext/Ret | `I_SensorAluminio` | Sensor **metal** | **2** |
| P3 vidrio | Ext/Ret | `I_SensorVidrio` | Sensor **vidrio** | **3** |
| — | — | — | — | **4** = desconocido (pass-through) |
| — | — | — | — | **0** = vacío / pendiente |

Tags extras: `I_SensorPieza` (entrada a báscula), `I_BasculaLista`, `DB_HMI.PesoActualKg`.

Memorias nuevas:
- `M_Pesando` — banda parada para peso + foto
- `M_ClasifPlastico/Aluminio/Vidrio` — ciclo pistón
- `M_Clasificando` — OR de clasif (para banda en empuje)
- `M_PassThrough` — visión=4, pieza debe salir sin pistón

---

## OB1
`FC_Modos` → `FC_Secuencia` → `FC_Alarmas` → `FC_EspejoWeb`

---

## FC_Modos
Igual que antes: Start/Stop/Emergencia → `M_SistemaOn` · `ModoAuto` · lámparas.

---

## FC_Secuencia

### NW1 — Entrada a báscula → `(S) M_Pesando`

Flanco de pieza en báscula (o `I_SensorPieza`):

```
---| M_SistemaOn |---| M_ModoAuto |---|/ Emergencia |---|/ M_Alarma |
---| I_SensorPieza |---|/ M_Pesando |---|/ M_Clasificando |
---| VisionMaterial == 0 |----(P) → (S) M_Pesando
```

> La Pi ve `M_Pesando` / `I_SensorPieza` y toma la foto + Gemini mientras la banda está quieta.

---

### NW2 — Banda AUTO // MANUAL → `Q_Banda`

```
AUTO: On · Auto · /Emerg · /Alarma · /M_Pesando · /M_Clasificando ─┐
                                                                   ├──( ) Q_Banda
MANUAL: On · /Auto · ManualBanda ──────────────────────────────────┘
```

`M_Pesando` **o** `M_Clasificando` cortan la banda.

---

### NW3 — Fin pesar+visión → `(R) M_Pesando` (banda puede seguir)

Cuando hay resultado de Gemini (**1..4**) y báscula lista:

```
---| M_Pesando |---| I_BasculaLista |---| VisionMaterial <> 0 |----(R) M_Pesando
```

Opcional: copiar peso a `DatosEstacion.PesoActualKg` aquí o en EspejoWeb.

---

### NW4 — Latch plástico (solo si IA dijo 1)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |---|/ M_Pesando |
---| VisionMaterial == 1 |---| I_SensorPlastico |
---|/ ClasifAluminio |---|/ ClasifVidrio |----(S) M_ClasifPlastico
```

> Si Vision=2 o 3, `I_SensorPlastico` puede activarse (detecta todo) y **se ignora**.

---

### NW5 — Latch latas (solo si IA dijo 2)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |---|/ M_Pesando |
---| VisionMaterial == 2 |---| I_SensorAluminio |
---|/ ClasifPlastico |---|/ ClasifVidrio |----(S) M_ClasifAluminio
```

---

### NW6 — Latch vidrio (solo si IA dijo 3)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |---|/ M_Pesando |
---| VisionMaterial == 3 |---| I_SensorVidrio |
---|/ ClasifPlastico |---|/ ClasifAluminio |----(S) M_ClasifVidrio
```

---

### NW7 — `M_Clasificando` + pass-through

```scl
M_Clasificando := M_ClasifPlastico OR M_ClasifAluminio OR M_ClasifVidrio;
M_PassThrough  := (DB_HMI.VisionMaterial = 4);
```

---

### NW8 — Desconocido: limpiar al salir (sensor final)

El óptico del final (`I_SensorPlastico`) marca que la pieza ya salió:

```
---| VisionMaterial == 4 |---| I_SensorPlastico |---- MOVE 0 → VisionMaterial
```

(Sin pistón, sin contador de material, o contador opcional “rechazo”.)

---

### NW9–11 — Comando pistón + Ext/Ret
Igual que antes (`M_PistonN` ← ClasifX // Manual; interlock Ext/Ret).

### NW12–14 — TON empuje → contar + `VisionMaterial := 0`
Igual que antes (solo para 1/2/3).

---

## FC_Alarmas

### Timeout: pesando sin visión
```
---| M_Pesando |----[ TON T_TimeoutVision  PT:=T#8s ]
---| T_TimeoutVision.Q |----(S) M_Alarma
---| T_TimeoutVision.Q |----(R) M_Pesando
---| T_TimeoutVision.Q |---- MOVE 4 → VisionMaterial   // tratar como pass-through
```
(O MOVE 0 y (R) Pesando para soltar la pieza sin clasificar — elegí uno y documentalo en la demo.)

### Timeout: visión 1/2/3 sin sensor de vía
```
---| VisionMaterial >= 1 |---| VisionMaterial <= 3 |---|/ M_Clasificando |---|/ M_Pesando |
----[ TON T_TimeoutVia  PT:=T#8s ]
---| T_TimeoutVia.Q |---- MOVE 0 → VisionMaterial   // deja pasar / libera
```

Emergencia: `(R) M_Pesando` + `(R) Clasif*` + `VisionMaterial := 0`.

---

## Quién escribe qué

| Campo | Quién |
|---|---|
| `VisionMaterial` 1/2/3/4 | Pi (`vision_gemini.py` en estación / classify HTTP en prueba) |
| `VisionMaterial := 0` | PLC al terminar empuje, pass-through, timeout o emergencia |
| `M_Pesando` | PLC (entrada pieza) |
| Foto | Pi cuando `M_Pesando` o flanco `I_SensorPieza` |

---

## Checklist mental demo

1. Pieza → banda para → foto+peso  
2. Gemini=lata → banda sigue → metal ON (aunque óptico final también parpadee) → **solo** metal para + P2  
3. Gemini=desconocido → banda sigue hasta el final → sin pistón  
