# Sin AS — solo Web ↔ TIA (atajo de laboratorio)

> **No es el camino de la rúbrica.** Sirve para probar HMI/bridge sin abrir AS.  
> En defensa: **Automation Studio + TIA + físico** son obligatorios; la web es **plus**.  
> Ver [`12_RUBRICA_CAPAS.md`](12_RUBRICA_CAPAS.md).

```
Estación (app) → Abrir HMI
        ↕ Firestore
   plc_bridge.py
        ↕
   PLC 1511C (PLCSIM) · M_PistonN + Ext/Ret · TON empuje
```

| Pistón | Material | Solenoides sim |
|---|---|---|
| P1 | Plástico | `M_Piston1Ext` / `M_Piston1Ret` |
| P2 | Latas (aluminio) | `M_Piston2Ext` / `M_Piston2Ret` |
| P3 | Vidrio | `M_Piston3Ext` / `M_Piston3Ret` |

**Sin** sensores de posición: ciclo AUTO con `T_EmpujePistonN`.  
Tags: `tia/TABLA_TAGS_DESDE_CERO.md` · Networks: `tia/NETWORKS_WEB_ONLY.md` · DB: `tia/MAPA_DB_HMI.md`

HMI **solo** desde la vista de estación (no hay icono en la barra superior).

Bridge:
```powershell
py plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
```

PLC real (6 Q solenoides + relés 220 V · 5 DI material): `plc_real/TABLA_IO_1214C.md`.
