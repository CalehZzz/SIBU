# Guacamayos — Arquitectura del sistema (reto Siemens 2026)

## Qué estás construyendo

Una **estación automatizada de clasificación y pesaje** de **plástico, latas (aluminio) y vidrio**, con:

| Pieza del reto | Herramienta | Rol | Rúbrica |
|---|---|---|---|
| PLC Siemens | **TIA Portal V20** + PLCSIM | Cerebro: arranque/paro, modos, secuencia, contadores, alarmas | **Obligatorio** |
| Electroneumática | **Automation Studio** | Simula banda, sensores, cilindro (pistón) y electroválvulas | **Obligatorio** |
| Físico | Mesa + PLC 1214C (I/Q) | Misma lógica en hardware real | **Obligatorio** (integral / campo) |
| HMI | WinCC **o** HMI web Guacamayos | ON/OFF, manual/auto, contadores, alarmas | **Obligatorio** |
| **Plus / innovación** | **App web Guacamayos** | Usuario + HMI remota; no sustituye AS ni TIA | **Plus** |

Ver mapa completo: [`12_RUBRICA_CAPAS.md`](12_RUBRICA_CAPAS.md).

## Flujo físico (3 materiales)

```
  [Entrada] → [Báscula] → [Banda] → sensores
                                      ├─ Plástico → P1
                                      ├─ Latas    → P2
                                      └─ Vidrio   → P3
```

HMI solo desde la estación conectada (botón Abrir HMI).

## Cómo se conecta TODO (importante)

El navegador **no puede hablar S7** directamente con el PLC. Por eso hay un puente:

```
  Automation Studio  ←→  (simulación física / I/O)
         │
         │  (en el reto: demuestras el circuito; el PLC se prueba en PLCSIM)
         ▼
  TIA Portal / PLCSIM (PLC virtual)
         │
         │  protocolo S7 (snap7)
         ▼
  plc_bridge.py  (Python en tu PC)
         │
         │  escribe Firestore
         ▼
  App web Guacamayos (index.html)
         │
         ▼
  Celular / laptop del usuario en la estación
```

### Contrato de datos (lo que la web espera)

Documento Firestore: `sesiones_activas/{estacionId}`

```json
{
  "materiales": {
    "plastico": { "piezas": 0, "pesoKg": 0.0 },
    "aluminio": { "piezas": 0, "pesoKg": 0.0 }
  },
  "finalizada": false,
  "plc": {
    "conectado": true,
    "sistemaOn": true,
    "modoAuto": true,
    "emergencia": false,
    "alarma": false,
    "estado": "running",
    "ultimoMaterial": "plastico",
    "banda": true,
    "piston": false,
    "pesoActualKg": 0.0
  },
  "actualizado": "<timestamp>"
}
```

- La **web crea** el documento en ceros al pulsar **Conectar**.
- El **bridge / simulador** lo actualiza leyendo el PLC (o simulando).
- Si `finalizada: true`, la web cierra la sesión y guarda el registro.

## Orden recomendado de trabajo

1. Leer esta arquitectura.
2. Seguir `01_GUIA_TIA_PORTAL.md` (crear proyecto → tags → programa → HMI → PLCSIM).
3. Seguir `02_GUIA_AUTOMATION_STUDIO.md` (circuito electroneumático).
4. Seguir `03_CONEXION_WEB_PLC.md` (bridge + app).
5. Usar `tia/MAPA_IO_Y_DB.md` como hoja de referencia mientras programas.

## Mapeo al enunciado del reto

| Requisito del PDF | Dónde lo cumples |
|---|---|
| Encendido / apagado | HMI + `M_SistemaOn` en PLC |
| Banda transportadora | `Q_Banda` + AS |
| Detección con sensores | `I_SensorPlastico`, `I_SensorAluminio`, `I_BasculaOK` |
| Clasificación neumática | `Q_Piston` + cilindro en AS |
| Conteo de piezas | Contadores `ContPlastico` / `ContAluminio` en DB |
| Modo manual / automático | HMI + lógica en FC |
| Paro de emergencia | `I_Emergencia` corta todo |
| HMI | Pantalla WinCC en TIA |
| Alarmas | `M_Alarma` + mensajes HMI |
| **Innovación** | App Guacamayos conectada al PLC vía bridge |
