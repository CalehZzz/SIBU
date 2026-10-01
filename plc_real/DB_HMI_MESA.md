# DB_HMI de la mesa (programa TIA real · distinto al contrato “SIBU docs”)

El LAD de la CPU **1215C** (`6ES7 215-1HG40-0XB0`) **no** usa el mismo mapa que `tia/MAPA_DB_HMI.md`.

## Confirmado en TIA (pantallazos)

### Control de banda
- **AUTO:** `M_SistemaOn` · `M_AutoActivo` · `M_PiezaEnProceso` · `/M_Standby` · `/M_Falla` · `/I_Emergencia` · `/M_ParadaInspeccion` · `/M_ParadaClasificacion` → `Q_Banda`
- **MANUAL:** `M_SistemaOn` · `M_ManualActivo` · (`M_ManualBanda` **OR** `DB_HMI.Manual_Banda`) · `/M_Falla` · `/I_Emergencia` → `Q_Banda`
- Bit HMI banda: **`%DB3.DBX1.3`** = `DB_HMI.Manual_Banda`

### Parada inspección
- Set `M_ParadaInspeccion`: `M_PiezaEnProceso` · `I_PiezaInspeccion`
- Reset: `I_AnalisisListo`
- Solo corta rama **AUTO** de banda (no la manual)

## Mapa bridge `--perfil mesa`

| Firestore / bridge | Offset mesa | Nota |
|---|---|---|
| `Start` | **0.0** | *asumido — confirmar en TIA* |
| `Stop` | **0.1** | *asumido* |
| `Emergencia` | **0.2** | *asumido (además hay `I_Emergencia` físico)* |
| `ResetAlarma` | **0.3** | *asumido* |
| `ModoAuto` | **0.4** | *asumido — debe relacionarse con `M_AutoActivo` / `M_ManualActivo`* |
| `FinSesion` | **0.5** | *asumido* |
| `ManualPiston` (P3) | **0.6** | *libre en docs viejos; mesa TBD* |
| `ManualBanda` | **1.3** | **CONFIRMADO** (`Manual_Banda`) |
| `ManualPiston1` | **1.4** | *asumido / TBD* |
| `ManualPiston2` | **1.5** | *asumido / TBD* |

> Si Start no pone `M_SistemaOn`, el offset de Start está mal o la red Set en TIA es otra. Mandá pantallazo de **toda la declaración de DB_HMI** y de la red que hace `(S) M_SistemaOn`.

## Condiciones para que Marcha mueva la banda

1. `M_SistemaOn` = 1  
2. `M_ManualActivo` = 1  
3. `DB_HMI.Manual_Banda` @ **1.3** = 1  
4. `M_Falla` = 0 · `I_Emergencia` = 0  

Sin (1) o (2), el bit de banda llega y la UI “vuelve a Parada”.
