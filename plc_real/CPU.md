# CPU física — SIBU Costa Rica

| Campo | Valor |
|---|---|
| **MLFB** | `6ES7 215-1HG40-0XB0` |
| **Nombre** | CPU **1215C** DC/DC/DC |
| **Firmware** | **V4.5** |
| DI / DQ | 14 DI · 10 DQ (24 V DC transistor) |
| AI | 2 |

## Implicaciones

1. En TIA Portal creá el device con **este MLFB** (no 1214C, no AC/DC/Rly).  
2. Las salidas `%Q` son **transistor 24 V DC** (no relé interno). Para lámparas 220 V / cargas AC usá **relés externos** accionados por `%Q`.  
3. Comunes de carga: alimentá el grupo de salidas según el manual de la 1215C (L+ / M).  
4. snap7: rack **0**, slot **1**, PUT/GET **ON**, DBs Optimized **OFF**.  
5. El contrato web (`DB1` / `DB3`) es el mismo que en sim; solo cambia el device.

I/O lógico: `TABLA_IO_1214C.md` (nombre histórico del archivo; aplica a esta 1215C).
