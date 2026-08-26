# Tabla maestra de tags — simulación HMI virtual (CPU 1511C · 3 pistones)

> **Modo actual (sin AS):** sensores y comandos van por **`DB_HMI`**.  
> Guía: `docs/11_SIN_AS_SOLO_WEB.md` · Networks: `tia/NETWORKS_WEB_ONLY.md`.  
> Mismo esquema de **3 pistones** que el PLC real (`plc_real/`), pero 100 % simulado.

HMI web SIBU → **`DB_HMI`** (vía bridge).  
Espejo a la web → **`DatosEstacion`**.

```
Página SIBU / HMI virtual (KTP700)
        ↕ Firestore  (estación: parque-central u otra)
   plc_bridge.py
        ↕ snap7
   PLC 1511C (PLCSIM) · M_Piston1/2/3 · DB_HMI sensores sim
```

---

## Grupos

| Grupo | Área | Uso |
|---|---|---|
| **B. Lógica interna** | `%M` | Sistema, modos, **3 pistones**, clasif |
| **C. DB_HMI (DB3)** | DB | Comandos + sensores simulados |
| **D. DatosEstacion (DB1)** | DB | Contadores / estado → web |
| **E. Timers** | TON IEC | Retardo y timeout P2 / P3 |

~~Grupo A (AS/KEP)~~ — no necesario en web-only.

---

## B) Lógica interna PLC (`%M` Bool)

| Nombre | Dirección ej. | Descripción |
|---|---|---|
| `M_SistemaOn` | `%M0.0` | Latch sistema ON |
| `M_ModoAuto` | `%M0.1` | Copia de `DB_HMI.ModoAuto` |
| `M_Alarma` | `%M0.2` | Alarma activa |
| `M_ClasifPlastico` | `%M0.3` | Secuencia empuje P2 |
| `M_ClasifAluminio` | `%M0.4` | Secuencia empuje P3 |
| `M_Clasificando` | `%M0.7` | OR de clasif (para banda / P1) |
| `M_Banda` | `%M3.0` | Actuador banda (sim) |
| `M_Piston1` | `%M3.1` | Comando P1 (deseo extendido) |
| `M_Piston2` | `%M3.2` | Comando P2 |
| `M_Piston3` | `%M3.3` | Comando P3 |
| `M_LamparaRun` | `%M3.4` | Piloto run (opc.) |
| `M_LamparaAlarma` | `%M3.5` | Piloto alarma |
| `M_LamparaEmergencia` | `%M3.6` | Piloto emergencia |
| `M_Piston1Ext` | `%M4.0` | Solenoide A P1 (sim) |
| `M_Piston1Ret` | `%M4.1` | Solenoide B P1 (sim) |
| `M_Piston2Ext` | `%M4.2` | Solenoide A P2 (sim) |
| `M_Piston2Ret` | `%M4.3` | Solenoide B P2 (sim) |
| `M_Piston3Ext` | `%M4.4` | Solenoide A P3 (sim) |
| `M_Piston3Ret` | `%M4.5` | Solenoide B P3 (sim) |

> Cilindros **doble efecto** (5/2 biestable): `M_PistonN` = comando; `M_PistonNExt`/`Ret` = solenoides.  
> FC sim: `DB_HMI.PistonNExtendido` (100 %) y `DB_HMI.PistonNRetractado` (0 %).

---

## C) `DB_HMI` — número **3** · Optimized **OFF** · ≥ **7 bytes**

| Nombre | Tipo | Offset | Control web |
|---|---|---|---|
| `Start` … `ManualPiston` | Bool | 0.0–0.7 | Operador (`ManualPiston` = P3) |
| `BasculaLista` | Bool | 1.0 | Sim báscula |
| `SensorPieza` | Bool | 1.1 | Sim pieza |
| `SensorPlastico` | Bool | 1.2 | Sim plástico |
| `SensorAluminio` | Bool | 1.3 | Sim aluminio |
| `Piston1Extendido` | Bool | 1.4 | Sim FC P1 **100 %** |
| `Piston2Extendido` | Bool | 1.5 | Sim FC P2 **100 %** |
| `ManualPiston1` | Bool | 1.6 | Manual P1 |
| `ManualPiston2` | Bool | 1.7 | Manual P2 |
| `PesoActualKg` | Real | **2.0** | Peso |
| `Piston3Extendido` | Bool | **6.0** | Sim FC P3 **100 %** |
| `SensorVidrio` | Bool | **6.1** | Sim vidrio |
| `Piston1Retractado` | Bool | **6.2** | Sim FC P1 **0 %** |
| `Piston2Retractado` | Bool | **6.3** | Sim FC P2 **0 %** |
| `Piston3Retractado` | Bool | **6.4** | Sim FC P3 **0 %** |

Detalle completo: `tia/MAPA_DB_HMI.md`.

---

## D) `DatosEstacion` — número **1** · Optimized **OFF** · **28 bytes**

| Nombre | Tipo | Offset | Descripción |
|---|---|---|---|
| `ContPlastico` | Int | 0.0 | Piezas plástico |
| `ContAluminio` | Int | 2.0 | Piezas aluminio (latas) |
| `PesoPlasticoKg` | Real | 4.0 | kg plástico |
| `PesoAluminioKg` | Real | 8.0 | kg latas |
| `PesoActualKg` | Real | 12.0 | último peso |
| `SesionActiva` | Bool | 16.0 | sesión abierta |
| `FinSesion` | Bool | 16.1 | fin → web |
| `SistemaOn` | Bool | 16.2 | espejo |
| `ModoAuto` | Bool | 16.3 | espejo |
| `Emergencia` | Bool | 16.4 | espejo |
| `Alarma` | Bool | 16.5 | espejo |
| `BandaOn` | Bool | 16.6 | espejo banda |
| `PistonOn` | Bool | 16.7 | OR de P1/P2/P3 |
| `Piston1On` | Bool | **17.0** | Plástico |
| `Piston2On` | Bool | **17.1** | Latas |
| `Piston3On` | Bool | **17.2** | Vidrio |
| `EstadoMaquina` | Int | 18.0 | 0…4 |
| `UltimoMaterial` | Int | 20.0 | 0/1/2/**3=vidrio** |
| `ContVidrio` | Int | **22.0** | Piezas vidrio |
| `PesoVidrioKg` | Real | **24.0** | kg vidrio |

Si el probe dice DB1 < 28 B o DB3 < 7 B → `plc_real/FIX_DB_INVALID_ADDRESS.md`.

## E) Timers

| Instancia | PT | Uso |
|---|---|---|
| `T_RetardoPiston2` | `T#500ms` | Contar plástico tras P2 extendido |
| `T_RetardoPiston3` | `T#500ms` | Contar aluminio tras P3 extendido |
| `T_TimeoutPiston2` | `T#3s` | Alarma si P2 no llega a 100% |
| `T_TimeoutPiston3` | `T#3s` | Alarma si P3 no llega a 100% |

---

## Mapa mental

```
HMI virtual (3× Extender/Retractar + sim sensores)
        ↓  hmi_comandos/{estación}
   plc_bridge.py
        ↓  DB_HMI
   FC_Modos / FC_Secuencia / FC_Alarmas
        ↓  M_Banda · M_Piston1/2/3 · M_PistonNExt/Ret
   FC_EspejoWeb → DatosEstacion → sesiones_activas → web
```

---

## Checklist TIA (sim)

1. Crear `DB_HMI` (DB3) y `DatosEstacion` (DB1) con offsets de arriba · Optimized **OFF**
2. Tags `%M` de los 3 comandos + 6 solenoides Ext/Ret + latches
3. Programar networks de `NETWORKS_WEB_ONLY.md`
4. Download a PLCSIM Advanced · PUT/GET ON
5. Bridge: `py plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3`
6. En la app: conectar estación → **Abrir HMI**
