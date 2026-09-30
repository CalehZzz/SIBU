# ESP32 RFID (legado — sketch solo)

> **Preferido:** un solo ESP32 con botes + báscula + RFID → [`../arduino_bins/`](../arduino_bins/).

Este sketch solo POST a `:8081`. Los pines VSPI (18/19/22) **chocan** con el sketch unificado; no lo uses en la misma placa que los HC/LCD.
