# Qué necesito para integrar la API de Gemini

Pasame esto y dejo el servicio de visión cerrado (sin placeholders).

## Obligatorio

1. **`GEMINI_API_KEY`**  
   - De [Google AI Studio](https://aistudio.google.com/apikey)  
   - La metemos solo en la Pi 4 (`export` / `.env`), nunca en `index.html` ni en Git.

2. **Modelo preferido** (si no decís, uso `gemini-2.0-flash` o el flash estable del momento)  
   - Ej.: `gemini-2.0-flash` / `gemini-1.5-flash`

3. **Cómo conectás la cámara a la Pi 4**  
   - CSI (módulo oficial) **o** USB  
   - Comando con el que ya sacás una foto (ej. `libcamera-still -o /tmp/shot.jpg` o OpenCV `/dev/video0`)

4. **IP del PLC** en la red de evaluación (o la de prueba ahora)

5. **Confirmación del contrato** (ya está en el repo):  
   - `DB_HMI` DB3 · `VisionMaterial` **Int @ offset 6.0**  
   - Valores: `0` ninguno · `1` plástico · `2` aluminio · `3` vidrio

## Muy útil (acelera el prompt)

6. **Lista exacta de materiales** del enunciado Costa Rica  
   - Nombres oficiales (ej. “botella PET”, “lata aluminio”, “vidrio”)

7. **2–3 fotos de ejemplo** por material (con la misma cámara / luz / ángulo de la demo)

8. **Trigger de captura**  
   - Default: flanco de `I_SensorPieza` leído por snap7  
   - Alternativa: captura continua cada N ms mientras `M_SistemaOn` y `VisionMaterial==0`

9. **Timeout** aceptable Gemini → PLC (default: si tarda > 3 s, no escribe y reintenta / log)

## Opcional

10. ¿Publicar también a Firestore `ultimoVision` para mostrarlo en la web? (sí/no)  
11. ¿Rack/slot distintos del 0/1 por defecto del 1214C?

## Qué ya está listo sin tu key

- Ladder: para solo con `VisionMaterial==N` **y** sensor físico de esa vía  
- DB contrato + checklist  
- Stub `vision_gemini.py` (falla claro si falta `GEMINI_API_KEY`)

Cuando tengas la key + tipo de cámara, el siguiente commit cierra el loop real captura→API→snap7.
