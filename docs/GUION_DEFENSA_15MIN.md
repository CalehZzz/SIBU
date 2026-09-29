# Guion de defensa — SIBU / Guacamayos  
**Siemens Youth Innovation Search 2026 · Primera Parte · 15 minutos**

Defensa **virtual**.  
- **Edgar** y **Caleb**: Colegio Don Bosco (pueden compartir pantalla desde sus PCs).  
- **Carla**: se conecta desde otro país (comparte su pantalla en su bloque).

**Jurado:** ingenieros Siemens (CR). Hablen técnico con seguridad; no “vendan humo”.

**Regla de timing:** los minutos abajo asumen habla natural (~140 palabras/min) + demos.  
Si un bloque de texto se siente largo al ensayar, **corten adornos**, no corten TIA ni la demo.

**Importante:** no adelanten el cierre físico. Eso aparece **solo al final**.

---

## Mapa rápido: criterios → quién los gana

| Criterio | Pts | Quién lo carga |
|---|---:|---|
| Análisis del problema y propuesta | 10 | **Edgar** |
| Programación PLC en TIA Portal | 20 | **Carla** |
| Simulación electroneumática en AS | 15 | **Carla** |
| Desarrollo de la HMI | 15 | **Caleb** |
| Funcionamiento integral | 15 | Los 3 (demo al final del bloque Caleb + cierre) |
| Innovación y mejora técnica | 10 | **Caleb** |
| Presentación técnica | 5 | Estructura de esta defensa |
| Presentación y habilidades comunicativas | 10 | **Edgar** abre; tono de los 3 |
| **TOTAL** | **100** | |

---

## Setup técnico (antes de entrar)

| Quién | Tiene abierto / listo |
|---|---|
| Edgar | 2–3 diapos: problema + sketch estación |
| Carla | TIA (OB1/FCs/DBs) + AS + KEP ya abiertos en ventanas |
| Caleb | Web/HMI lista; pantalla completa ensayada |
| Los 3 | Zoom/Meet estable · mics · **mesa física lista pero fuera de cámara hasta el final** |

**Regla virtual:** quien habla, comparte pantalla. Handoff &lt; 10 s.

---

## Reloj realista (15:00)

| Min | Dur | Bloque | Quién |
|---:|---:|---|---|
| 0:00–0:35 | 35s | Hook | Edgar |
| 0:35–0:50 | 15s | Quiénes somos | Edgar |
| 0:50–1:30 | 40s | Problemática (3 fallas) | Edgar |
| 1:30–2:15 | 45s | Propuesta + 3 capas | Edgar |
| 2:15–2:25 | 10s | Handoff → Carla | Edgar |
| 2:25–3:00 | 35s | Stack / diagrama AS–KEP–TIA | Carla |
| 3:00–4:45 | 1:45 | TIA: FCs + detalle + 30–40s mostrando LAD | Carla |
| 4:45–5:50 | 1:05 | AS + KEP (40s habla + 25s mostrar circuito) | Carla |
| 5:50–6:00 | 10s | Handoff → Caleb | Carla |
| 6:00–6:40 | 40s | Innovación: por qué web | Caleb |
| 6:40–8:10 | 1:30 | HMI: explicación 40s + clicks en vivo 50s | Caleb |
| 8:10–8:35 | 25s | 3 mejoras técnicas | Caleb |
| 8:35–9:00 | 25s | Ciclo integral (narrado) | Caleb |
| 9:00–12:30 | 3:30 | **Demo integral en vivo** (PLC/AS/web según lo que corra) | Caleb + Carla |
| 12:30–14:20 | 1:50 | **Cierre físico** (primera mención) | Edgar + Caleb (+ Carla 10s) |
| 14:20–15:00 | 40s | Cierre + gracias | Edgar (+ 1 línea c/u) |

Si la demo integral se atrasa: recorten el tramo 9:00–12:30, **no** el físico ni el cierre.

---

# BLOQUE 1 — Edgar (0:00 → 2:25)

### 0:00–0:35 · Hook (~35 s)

**Edgar (cámara; 1 imagen fuerte opcional):**

> “Ingenieros: imaginen un colegio en San José un viernes a las 2.  
> Hay un contenedor de reciclaje… y al lado, botellas y latas en el piso.  
> No es que la gente no sepa reciclar. Es que **reciclar no da feedback**: no pesa, no clasifica, no premia.  
> Hoy les presentamos **SIBU**: una estación que convierte el reciclaje en un proceso medible y en una recompensa.”

### 0:35–0:50 · Equipo (~15 s)

> “Equipo Guacamayos, Colegio Don Bosco.  
> Yo soy **Edgar**. Conmigo está **Caleb**.  
> En remoto, **Carla**, que les abre el núcleo Siemens.”

### 0:50–1:30 · Problemática (~40 s)

> “En la práctica el reciclaje escolar y comunitario falla en tres cosas de ingeniería:  
> uno, **clasificación inconsistente**;  
> dos, **sin medición** — ni peso ni sesión;  
> tres, **sin incentivo cerrado** — el usuario no ve resultado.  
> Eso no se arregla con un afiche. Se arregla con automatización y una interfaz que sí se use.”

### 1:30–2:15 · Propuesta (~45 s)

> “Propuesta: una **estación modular SIBU** para colegio, parque o centro comunal.  
> Flujo: pieza → báscula → banda → sensor de material → separación → app con piezas, kilos y colones.  
> Lo que pide la rúbrica: electroneumática en **Automation Studio**, cerebro en **TIA Portal**, y al final **físico** en mesa.  
> Nuestro **plus**: **HMI/app web** que habla con el mismo PLC — no sustituye AS ni el hardware.  
> Carla: PLC y circuito. Caleb: el plus que toca el usuario.”

### 2:15–2:25 · Handoff (~10 s)

> “Carla, te paso el lead: LAD, timers y el bus de datos.”

---

# BLOQUE 2 — Carla (2:25 → 6:00)

### 2:25–3:00 · Stack (~35 s)

**Carla (diagrama AS ↔ KEP ↔ TIA):**

> “Soy Carla.  
> Stack: **TIA V20**, CPU **1511C** en **PLCSIM Advanced**, **Automation Studio 10**, **KEPServerEX 6**.  
> AS no habla S7 a gusto; KEP mueve tags a memorias del PLC.  
> En sim usamos **`%M`** con AS.  
> La web escribe **`DB_HMI`** y lee **`DatosEstacion`** — no pisa I/Q a ciegas.”

### 3:00–4:45 · TIA Portal (~1:45 = ~60–70 s habla + 30–40 s mostrando)

**Carla (OB1 + FCs en pantalla):**

> “Cuatro funciones:  
> **`FC_Modos`** — Start, Stop, emergencia, auto.  
> **`FC_Secuencia`** — banda, material, clasificación, conteo.  
> **`FC_Alarmas`** — timeout y sensores contradictorios.  
> **`FC_EspejoWeb`** — estado a `DatosEstacion`.  
>  
> Detalles que importan: una bobina por actuador con **AUTO // MANUAL en paralelo**; **TON IEC** de retardo y timeout; DBs con **Optimized OFF** por offsets de snap7; comandos en **DB3**, estado en **DB1**.”

*(Ahora sí: 30–40 s ONLINE / un network grande / un ciclo corto. No narren encima todo el tiempo.)*

### 4:45–5:50 · AS + KEP (~1:05)

**Carla (AS visible):**

> “En AS: aire → **5/2** → cilindro doble efecto → sensores **0% / 100%**.  
> KEP enlaza a tags del PLC.  
> El PLC manda solenoide; AS mueve; el sensor vuelve; la secuencia avanza.  
> Feedback de proceso, no animación suelta.”

*(25 s mostrando el cilindro moverse o el enlace de tags.)*

### 5:50–6:00 · Handoff (~10 s)

> “Cerebro Siemens y músculo neumático listos.  
> Caleb: la capa que el usuario toca.”

---

# BLOQUE 3 — Caleb (6:00 → 9:00)

### 6:00–6:40 · Innovación (~40 s)

**Caleb (abre la web):**

> “La rúbrica pide AS, TIA y físico. Nuestro **plus** es la web.  
> No otra KTP en el rack: en colegio o parque la gente trae **celular**.  
> **SIBU web**: app con kilos y colones; **HMI virtual**; puente **`plc_bridge.py`** Firestore ↔ snap7 ↔ DBs.  
> El PLC sigue siendo el master. El navegador no habla S7. El bridge sí.”

### 6:40–8:10 · HMI (~1:30)

**Habla (~40 s):**

> “En la HMI: Start, Stop, emergencia, auto/manual, peso.  
> Comandos a `DB_HMI`. Estado desde `DatosEstacion`: banda, pistones, contadores, alarmas.  
> Al usuario le mostramos progreso y recompensa, no tags. Misma verdad de proceso, dos lenguajes.”

**Demo clicks (~50 s):** pantalla completa → Start → un ciclo plástico o aluminio → señalar contadores. Pocas palabras mientras clickean.

### 8:10–8:35 · Tres mejoras (~25 s)

> “Uno: HMI usable en remoto y en sitio.  
> Dos: contrato de datos con offsets fijos.  
> Tres: la app no se rehace si cambiamos de PLCSIM a hardware.  
> Innovación aplicada a Siemens, no una landing.”

### 8:35–9:00 · Ciclo integral narrado (~25 s)

> “Ciclo: pieza → peso → banda → material → clasificación → conteo → recompensa.  
> Timeout o sensores cruzados: alarma. Emergencia: paro.  
> Un sistema, no tres demos sueltas. Ahora lo corremos junto.”

---

# BLOQUE 4 — Demo integral (9:00 → 12:30) · ~3:30

**Objetivo:** que el jurado *vea* TIA/AS/web alineados.  
No lean guion aquí. Solo cues:

1. Caleb: “Arrancamos sesión / Start.”  
2. Carla (si AS/TIA en su pantalla, o Caleb si todo está en una): ciclo plástico.  
3. Ciclo aluminio / clasificación.  
4. Señalar contador + app.  
5. (Opcional 15 s) emergencia o alarma.

Si algo falla: pasen al siguiente material; no depuren en vivo más de 20 s.

---

# BLOQUE 5 — Cierre físico (12:30 → 14:20) · primera mención

**Aquí es la primera vez que hablan de llevarlo a mesa física.**

**Edgar (~20 s):**

> “Hasta ahora vieron el sistema en simulación y software.  
> La pregunta de todo jurado Siemens es: ¿esto baja a campo?”

**Caleb (~40–50 s, cámara a la mesa / video corto):**

> “Sí. El **físico** también es rúbrica: banda, sensores y **tres pistones** — plástico, latas, vidrio.  
> Misma lógica Siemens en I/Q reales. La web es el plus de mando/usuario, no el único sistema.”

**Carla (~10–15 s):**

> “Pasamos de `%M`/AS a **I/Q** en **1214C**, mismo contrato de DBs.”

Si el hardware está a medias, digan exactamente eso — sin overclaim.

---

# BLOQUE 6 — Cierre (14:20 → 15:00) · ~40 s

**Edgar (~25 s):**

> “SIBU responde a un problema real de reciclaje con automatización: clasifica, mide y recompensa.  
> Núcleo Siemens: **AS + TIA + físico**. Web como **plus** al usuario.  
> Estación pensada para colegios y espacios públicos en Costa Rica.”

**Una línea cada uno (~15 s total):**

- **Carla:** “PLC y AS listos; físico en 1214C.”  
- **Caleb:** “Plus web: se toca con el celular.”  
- **Edgar:** “Gracias. Quedamos abiertos a preguntas técnicas.”

---

## Frases prohibidas

- “Es como un Arduino pero con Siemens…”  
- “La página es lo más importante y el PLC es secundario…”  
- “Todo está en la web; Automation Studio es opcional…”  
- “KEPServer es un programa que conecta cosas” → digan **tags / OPC / `%M`**  
- Cualquier “al final tenemos una sorpresa…” **antes** del minuto 12:30  

## Frases que suman con jurado Siemens

- “Rúbrica: AS + TIA + físico; la web es nuestro plus.”  
- “Optimized block access OFF por offsets de snap7.”  
- “Una bobina por salida: AUTO y MANUAL en paralelo.”  
- “Timeout TON de pistón → alarma.”  
- “AS da el feedback de posición; el PLC decide.”  
- “La HMI web escribe `DB_HMI`; no reemplaza la lógica del PLC.”  

---

## Checklist de ensayo (cronometrar en serio)

- [ ] Edgar bloque completo ≤ **2:25**  
- [ ] Carla habla+shows ≤ **3:35** (hasta handoff)  
- [ ] Caleb hasta ciclo integral ≤ **3:00**  
- [ ] Demo integral ensayada una vez sin guion  
- [ ] Físico **no** aparece en cámara antes del 12:30  
- [ ] Cierre a los **14:55**  

---

## Preguntas típicas (respuestas de 15–20 s)

**¿Por qué web y no solo Comfort Panel?**  
Usuario de colegio/parque trae celular; operador igual puede usar HMI web. El PLC sigue siendo el master.

**¿Si se cae internet?**  
El PLC sigue local; se pierde telemetría/app. En sitio: bridge en PC de estación + red local.

**¿Seguridad?**  
Stop/emergencia/timeout en lógica; E-stop cableado cuando el hardware lo permita.

**¿Solo plástico y aluminio?**  
Alcance del prototipo a propósito; tags/contadores extensibles.

**¿Aporte de cada quien?**  
Edgar: problema/propuesta. Carla: TIA+AS+KEP. Caleb: web/HMI/bridge.

---

## Cue cards (segunda pantalla)

### Edgar
1. Hook 35s  
2. 3 fallas 40s  
3. Estación + 3 capas 45s  
4. Paso a Carla  
5. *(al final)* ¿Baja a campo? → cierre  

### Carla
1. Stack 35s  
2. 4 FCs + Optimized OFF  
3. Mostrar LAD 30–40s  
4. AS 5/2 + 0/100  
5. Paso a Caleb  

### Caleb
1. Por qué web 40s  
2. HMI + clicks 1:30  
3. 3 mejoras 25s  
4. Ciclo + demo integral  
5. *(solo al final)* mesa física 3 pistones  
