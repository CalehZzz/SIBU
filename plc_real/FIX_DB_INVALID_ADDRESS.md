# Fix: `Invalid address (0x05)` / DB_HMI demasiado pequeño

Esos avisos del bridge **no son de la web**: el PLC tiene los DB más chicos (o Optimized ON / no descargados) de lo que pide el contrato.

| Síntoma | Qué pasa en el PLC |
|---|---|
| `lectura DatosEstacion: … Invalid address (0x05)` | DB1 no se puede leer a **28 bytes** |
| `DB_HMI … demasiado pequeño … solo bools (2 bytes)` | DB3 solo tiene ~**2 bytes** (falta Real @2.0) |

Contrato: `plc_real/DB_CONTRATO_WEB.md` · `tia/MAPA_DB_HMI.md`

---

## 1) Verificar con probe

```powershell
py plc_probe.py --ip 192.168.0.10
```

- **DB1** ≥ **28** → `DatosEstacion`
- **DB3** ≥ **8** → `DB_HMI` (incluye `VisionMaterial` Int @ 6.0)

---

## 2) Arreglar `DB_HMI` (DB **3**) en TIA

1. Optimized block access = **OFF**
2. Campos (offsets absolutos):

| Offset | Nombre | Tipo |
|---|---|---|
| 0.0–0.7 | Start … ManualPiston | Bool |
| 1.0–1.3 | BasculaLista … SensorAluminio | Bool |
| **1.4** | **ManualPiston1** | Bool |
| **1.5** | **ManualPiston2** | Bool |
| **1.6** | **SensorVidrio** | Bool |
| **2.0** | **PesoActualKg** | **Real** |
| **6.0** | **VisionMaterial** | **Int** (0/1/2/3) |

3. **No** crear `PistonNExtendido` / `PistonNRetractado` (pistones sin FC).  
   Si existen del diseño viejo, elimínalos del DB.
4. Tamaño **≥ 8 bytes** (`VisionMaterial` @ 6.0) → Download + RUN

---

## 3) Arreglar `DatosEstacion` (DB **1**)

Optimized **OFF** · hasta `PesoVidrioKg` @24 · **≥ 28 bytes** · Download + RUN

---

## 4) PUT/GET

CPU → Protection → **Permitir acceso PUT/GET** → Download hardware.

---

## 5) Reiniciar bridge

```powershell
py plc_real/plc_bridge_real.py --ip 192.168.0.10
```
