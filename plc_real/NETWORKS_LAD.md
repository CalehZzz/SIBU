# Networks LAD — PLC real · Gemini + 3 sensores (IA manda) · **sin espera de peso**

## Lógica física (actual)

```
[Entrada] I_SensorPieza → trigger foto Gemini (banda SIGUE; NO se espera peso)
       │
       ▼  VisionMaterial listo (1/2/3/4) mientras la pieza avanza
[Banda sigue]
       │
       ├─ Vision=1 (plástico) → espera SOLO I_SensorPlastico → PARA → P1
       ├─ Vision=2 (lata)     → espera SOLO I_SensorAluminio → PARA → P2
       ├─ Vision=3 (vidrio)   → espera SOLO I_SensorVidrio   → PARA → P3
       └─ Vision=4 (desconocido) → pass-through sin pistón
                                    → al FINAL de la banda: báscula DEMO
                                      (solo representación; poca importancia)
```

**Reglas:**
1. Gemini decide el material (`VisionMaterial`). Los sensores **no clasifican**.
2. **No hay paro para pesar.** La báscula **no** condiciona el ciclo de clasificación.
3. Pueden activarse varios sensores a la vez: **se ignoran** si no son el de la vía pedida por la IA.
4. La banda **solo** para por: (a) sensor de la vía del material detectado + pistón, (b) stop/alarma/emergencia.
5. `desconocido` (4) o timeout → **dejar pasar**, sin pistón. La báscula al final es cosmético / demo.

| Pistón | Ext/Ret | Sensor físico | Qué detecta el hardware | VisionMaterial |
|---|---|---|---|---|
| P1 plástico | Ext/Ret | `I_SensorPlastico` | Óptico “detecta todo” (vía P1) | **1** |
| P2 latas | Ext/Ret | `I_SensorAluminio` | Sensor **metal** | **2** |
| P3 vidrio | Ext/Ret | `I_SensorVidrio` | Sensor **vidrio** | **3** |
| — | — | — | — | **4** = desconocido (pass-through) |
| — | — | — | — | **0** = vacío / pendiente |

Tags:
- `I_SensorPieza` — óptico de **entrada** (trigger cámara; **no** para la banda por peso)
- `I_BasculaFinal` (`%I0.4`) — báscula al **final** (vía desconocidos / demo). **Opcional**, no gatea lógica
- `DB_HMI.PesoActualKg` — solo si querés mostrar un número en la web (demo)

Memorias:
- `M_EsperandoVision` — pieza detectada, aún `VisionMaterial==0` (banda **sigue**)
- `M_ClasifPlastico/Aluminio/Vidrio` — ciclo pistón
- `M_Clasificando` — OR de clasif (para banda en empuje)
- `M_PassThrough` — visión=4

> ~~`M_Pesando`~~ ya **no** se usa como paro por báscula. Si lo tenías, renombralo a `M_EsperandoVision` y **sacalo** de la red de banda.

---

## OB1
`FC_Modos` → `FC_Secuencia` → `FC_Alarmas` → `FC_EspejoWeb`

---

## FC_Modos
Start/Stop/Emergencia → `M_SistemaOn` · `ModoAuto` · lámparas.

---

## FC_Secuencia

### NW1 — Entrada → `(S) M_EsperandoVision` (banda **no** para)

Flanco de pieza a la entrada (`I_SensorPieza`):

```
---| M_SistemaOn |---| M_ModoAuto |---|/ Emergencia |---|/ M_Alarma |
---| I_SensorPieza |---|/ M_EsperandoVision |---|/ M_Clasificando |
---| VisionMaterial == 0 |----(P) → (S) M_EsperandoVision
```

> La Pi ve flanco `I_SensorPieza` / `M_EsperandoVision` y toma foto + Gemini **con la banda en marcha**.

---

### NW2 — Banda AUTO // MANUAL → `Q_Banda`

```
AUTO: On · Auto · /Emerg · /Alarma · /M_Clasificando ─┐
                                                      ├──( ) Q_Banda
MANUAL: On · /Auto · ManualBanda ─────────────────────┘
```

**Solo** `M_Clasificando` (empuje) corta la banda.  
**No** uses báscula ni `M_EsperandoVision` para cortar.

---

### NW3 — Visión lista → `(R) M_EsperandoVision`

Cuando Gemini escribió **1..4** (o timeout → 4):

```
---| M_EsperandoVision |---| VisionMaterial <> 0 |----(R) M_EsperandoVision
```

**No** esperar `I_BasculaLista` / peso.

---

### NW4 — Latch plástico (solo si IA dijo 1)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |
---| VisionMaterial == 1 |---| I_SensorPlastico |
---|/ ClasifAluminio |---|/ ClasifVidrio |----(S) M_ClasifPlastico
```

---

### NW5 — Latch latas (solo si IA dijo 2)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |
---| VisionMaterial == 2 |---| I_SensorAluminio |
---|/ ClasifPlastico |---|/ ClasifVidrio |----(S) M_ClasifAluminio
```

---

### NW6 — Latch vidrio (solo si IA dijo 3)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |
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

### NW8 — Desconocido: limpiar al llegar al final

Óptico final de vía o sensor de salida (ej. el mismo “detecta todo” al final, o un `I_SalidaFinal` si lo cableás):

```
---| VisionMaterial == 4 |---| I_SensorPlastico |---- MOVE 0 → VisionMaterial
```

(Sin pistón. Contador “rechazo” opcional.)

---

### NW8b — Báscula DEMO al final (opcional · poca importancia)

Solo representación: cuando un desconocido (o cualquier pieza que llegó al final) activa la báscula:

```
---| I_BasculaFinal |---- // opcional: copiar peso a DatosEstacion.PesoActualKg
                         // NO (R)/(S) de clasificación
                         // NO cortar banda por esto
```

Podés **omitir** esta red en la demo si no cableás báscula.

---

### NW9–11 — Comando pistón + Ext/Ret
`M_PistonN` ← ClasifX // Manual; interlock Ext/Ret (nunca ambos a 1).

### NW12–14 — TON empuje → contar + `VisionMaterial := 0`
Solo para 1/2/3.

---

## FC_Alarmas

### Timeout: entrada sin visión
```
---| M_EsperandoVision |----[ TON T_TimeoutVision  PT:=T#8s ]
---| T_TimeoutVision.Q |---- MOVE 4 → VisionMaterial   // pass-through
---| T_TimeoutVision.Q |----(R) M_EsperandoVision
```
(Opcional: alarma suave; no hace falta parar la planta.)

### Timeout: visión 1/2/3 sin sensor de vía
```
---| VisionMaterial >= 1 |---| VisionMaterial <= 3 |---|/ M_Clasificando |
----[ TON T_TimeoutVia  PT:=T#8s ]
---| T_TimeoutVia.Q |---- MOVE 0 → VisionMaterial   // deja pasar
```

Emergencia: `(R) M_EsperandoVision` + `(R) Clasif*` + `VisionMaterial := 0`.

---

## Quién escribe qué

| Campo | Quién |
|---|---|
| `VisionMaterial` 1/2/3/4 | Pi (`vision_gemini.py` / classify) |
| `VisionMaterial := 0` | PLC al terminar empuje, pass-through, timeout o emergencia |
| `M_EsperandoVision` | PLC (entrada pieza) — **no para banda** |
| Foto | Pi en flanco `I_SensorPieza` / `M_EsperandoVision` |
| Peso | Opcional demo vía `I_BasculaFinal` → web; **no** gatea |

---

## Checklist mental demo

1. Pieza entra → **banda sigue** → foto Gemini en marcha  
2. Gemini=lata → metal ON → **solo** metal para + P2  
3. Gemini=desconocido → pasa al final → báscula demo (si está) · sin pistón  
4. **Nunca** esperás “peso listo” para clasificar  
