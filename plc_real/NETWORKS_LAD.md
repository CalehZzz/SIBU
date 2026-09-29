# Networks LAD — PLC real · Gemini + botes HC-SR04 · **sin espera de peso**

## Lógica física (actual)

```
[Entrada] I_SensorPieza → trigger foto Gemini (banda SIGUE; NO se espera peso)
       │
       ▼  VisionMaterial listo (1/2/3/4) mientras la pieza avanza
[Banda sigue]
       │
       ├─ Vision=1 + bote plástico OK     → I_SensorPlastico → PARA → P1 → contador físico
       ├─ Vision=1 + bote plástico LLENO  → desvío a RECHAZO (como Vision=4) · lámpara AMARILLA
       ├─ Vision=2 + bote latas OK/LLENO  → igual (P2 o desvío)
       ├─ Vision=3 + bote vidrio OK/LLENO → igual (P3 o desvío)
       ├─ Vision=4 (desconocido)          → pass-through → rechazo
       └─ Bote RECHAZO LLENO              → lámpara ROJA · STOP TOTAL (banda off, sin clasificar)
```

**Reglas:**
1. Gemini decide el material (`VisionMaterial`). Los sensores de vía **no clasifican**.
2. **No hay paro para pesar.**
3. **Botes (Arduino HC-SR04):** si un material está lleno → **amarillo** y ese material va a rechazo; los otros 2 siguen.
4. Si el bote de **rechazo** está lleno → **rojo** y se **detiene todo**.
5. Contadores de mesa = pulsos `Q_Cont*` al clasificar (no al desviar).

| Pistón | Sensor vía | Vision | Si bote material lleno |
|---|---|---|---|
| P1 | `I_SensorPlastico` | 1 | Desvío rechazo · no P1 · no contador P |
| P2 | `I_SensorAluminio` | 2 | Desvío rechazo · no P2 |
| P3 | `I_SensorVidrio` | 3 | Desvío rechazo · no P3 |
| — | — | 4 | Siempre rechazo |

Señales de bote (Arduino → PLC DI o Merker vía bridge):
- `I_BinFullPlastico` / `Aluminio` / `Vidrio` / `Rechazo`

Memorias:
- `M_EsperandoVision` · `M_Clasif*` · `M_Clasificando`
- `M_DivertPlastico` := BinFullP AND NOT BinFullRechazo (idem A/V)
- `M_PassThrough` := Vision=4 OR DivertP OR DivertA OR DivertV
- `M_StopBotes` := BinFullRechazo → fuerza alarma/stop

Lámparas:
- `Q_LamparaRun` (verde) — sistema OK
- `Q_LamparaAmarilla` — algún bote P/A/V lleno (modo desvío)
- `Q_LamparaAlarma` (roja) — rechazo lleno **o** emergencia

---

## OB1
`FC_Modos` → `FC_Secuencia` → `FC_Alarmas` → `FC_EspejoWeb`

---

## FC_Modos

```
Q_LamparaAlarma (roja) := Emergencia OR M_Alarma OR I_BinFullRechazo;
Q_LamparaAmarilla      := NOT I_BinFullRechazo AND (I_BinFullPlastico OR I_BinFullAluminio OR I_BinFullVidrio);
Q_LamparaRun (verde)   := M_SistemaOn AND NOT Q_LamparaAlarma AND NOT Q_LamparaAmarilla;
```

`M_StopBotes` := `I_BinFullRechazo`

---

## FC_Secuencia

### NW1 — Entrada → `(S) M_EsperandoVision`

```
---| M_SistemaOn |---| M_ModoAuto |---|/ Emergencia |---|/ M_Alarma |---|/ M_StopBotes |
---| I_SensorPieza |---|/ M_EsperandoVision |---|/ M_Clasificando |
---| VisionMaterial == 0 |----(P) → (S) M_EsperandoVision
```

### NW2 — Banda → `Q_Banda`

```
AUTO: On · Auto · /Emerg · /Alarma · /M_StopBotes · /M_Clasificando ─┐
                                                                     ├──( ) Q_Banda
MANUAL: On · /Auto · ManualBanda · /M_StopBotes ─────────────────────┘
```

### NW3 — Visión lista → `(R) M_EsperandoVision`

```
---| M_EsperandoVision |---| VisionMaterial <> 0 |----(R) M_EsperandoVision
```

### NW3b — Flags de desvío

```scl
M_DivertPlastico := I_BinFullPlastico AND NOT I_BinFullRechazo;
M_DivertAluminio := I_BinFullAluminio AND NOT I_BinFullRechazo;
M_DivertVidrio   := I_BinFullVidrio   AND NOT I_BinFullRechazo;
M_PassThrough    := (VisionMaterial = 4)
                 OR (VisionMaterial = 1 AND M_DivertPlastico)
                 OR (VisionMaterial = 2 AND M_DivertAluminio)
                 OR (VisionMaterial = 3 AND M_DivertVidrio);
```

### NW4 — Latch plástico (IA=1 y bote P OK)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |---|/ M_StopBotes |---|/ M_DivertPlastico |
---| VisionMaterial == 1 |---| I_SensorPlastico |
---|/ ClasifAluminio |---|/ ClasifVidrio |----(S) M_ClasifPlastico
```

### NW5 — Latch latas (IA=2 y no divert)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |---|/ M_StopBotes |---|/ M_DivertAluminio |
---| VisionMaterial == 2 |---| I_SensorAluminio |
---|/ ClasifPlastico |---|/ ClasifVidrio |----(S) M_ClasifAluminio
```

### NW6 — Latch vidrio (IA=3 y no divert)

```
---| On |---| Auto |---|/ Emerg |---|/ Alarma |---|/ M_StopBotes |---|/ M_DivertVidrio |
---| VisionMaterial == 3 |---| I_SensorVidrio |
---|/ ClasifPlastico |---|/ ClasifAluminio |----(S) M_ClasifVidrio
```

### NW7 — `M_Clasificando`

```scl
M_Clasificando := M_ClasifPlastico OR M_ClasifAluminio OR M_ClasifVidrio;
```

### NW8 — Pass-through / desvío: limpiar Vision al final

```
---| M_PassThrough |---| I_SensorPlastico |---- MOVE 0 → VisionMaterial
```

### NW8b — Báscula DEMO al final (opcional)

### NW9–11 — Pistón Ext/Ret
### NW12–14 — TON → pulso contador FÍSICO + Vision:=0 (solo clasif real)

---

## FC_Alarmas

Timeout visión/vía como antes.

### Rechazo lleno
```
---| I_BinFullRechazo |----(S) M_Alarma
---| I_BinFullRechazo |----(R) M_EsperandoVision
---| I_BinFullRechazo |----(R) Clasif*
---| I_BinFullRechazo |---- MOVE 0 → VisionMaterial
```

---

## Quién escribe qué

| Campo | Quién |
|---|---|
| `VisionMaterial` | Pi Gemini |
| Bin full DI | Arduino HC-SR04 |
| Contadores mesa | `Q_Cont*` pulso |
| Panel web % | Firestore `bins_pi/estado` |

## Checklist demo

1. Plástico OK → P1 + contador físico  
2. Plástico lleno · latas OK → amarillo · plástico a rechazo · latas a P2  
3. Rechazo lleno → rojo · todo parado  
