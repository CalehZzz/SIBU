# Fix: `Invalid address (0x05)` / DB_HMI demasiado pequeño

Esos avisos del bridge **no son de la web**: el PLC tiene los DB más chicos (o Optimized ON / no descargados) de lo que pide el contrato.

| Síntoma | Qué pasa en el PLC |
|---|---|
| `lectura DatosEstacion: … Invalid address (0x05)` | DB1 no se puede leer a **28 bytes** (inexistente, Optimized ON, o demasiado corto) |
| `DB_HMI … demasiado pequeño … solo bools (2 bytes)` | DB3 solo tiene ~**2 bytes** (faltan Real @2.0 y bools @6.x) |

Contrato: `plc_real/DB_CONTRATO_WEB.md` · `tia/MAPA_DB_HMI.md`

---

## 1) Verificar con probe (misma IP del bridge)

```powershell
# PLC real
py plc_probe.py --ip 192.168.0.10

# PLCSIM Advanced (demo)
py plc_probe.py --ip 192.168.0.1
```

Busca:

- **DB1** legible hasta **≥ 28** → `DatosEstacion`
- **DB3** legible hasta **≥ 7** → `DB_HMI`

Si salen otros números, el bridge puede usar `--db N --db-hmi M`.

---

## 2) Arreglar `DB_HMI` (DB **3**) en TIA

1. Abre el DB → **Optimized block access = OFF**
2. Asegura estos campos (offsets absolutos):

| Offset | Nombre | Tipo |
|---|---|---|
| 0.0–0.7 | Start … ManualPiston | Bool |
| 1.0–1.7 | BasculaLista … ManualPiston2 | Bool |
| **2.0** | **PesoActualKg** | **Real** |
| **6.0** | **Piston3Extendido** | Bool (FC 100 %) |
| **6.1** | **SensorVidrio** | Bool |
| **6.2** | **Piston1Retractado** | Bool (FC 0 %) |
| **6.3** | **Piston2Retractado** | Bool |
| **6.4** | **Piston3Retractado** | Bool |

3. El tamaño del DB debe ser **≥ 7 bytes** (TIA lo calcula al compilar).
4. **Download** software a la CPU / instancia correcta → CPU **RUN**.

---

## 3) Arreglar `DatosEstacion` (DB **1**) en TIA

1. Optimized **OFF**
2. Campos hasta:

| Offset | Nombre | Tipo |
|---|---|---|
| … | (bools/contadores base) | … |
| 17.0–17.2 | Piston1On / Piston2On / Piston3On | Bool |
| 18.0 | EstadoMaquina | Int |
| 20.0 | UltimoMaterial | Int |
| **22.0** | **ContVidrio** | **Int** |
| **24.0** | **PesoVidrioKg** | **Real** |

3. Tamaño **≥ 28 bytes**
4. Download + RUN

---

## 4) PUT/GET (si ni Merker responde)

CPU → Properties → Protection → **Permitir acceso PUT/GET** a datos remotos → **Download hardware**.

---

## 5) Reiniciar bridge

```powershell
py plc_real/plc_bridge_real.py --ip 192.168.0.10
# o demo:
py plc_bridge.py parque-central --ip 192.168.0.1 --db 1 --db-hmi 3
```

El diagnóstico al inicio debe decir **OK** en ambos DB. Si aún está parcial, el bridge avisa **una vez** (no cada 0.3 s) y opera con lo que pueda hasta que descargues el DB completo.
