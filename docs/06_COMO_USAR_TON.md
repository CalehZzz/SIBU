# Cómo usar el TON en TIA Portal (S7-1200) — sin enredarte

> **Diseño actual:** pistones **sin** FC. El TON de empuje (`T_EmpujePistonN`) se dispara con `M_Clasif*`  
> (no con `I_PistonExtendido`). Ver `tia/NETWORKS_WEB_ONLY.md` / `plc_real/NETWORKS_LAD.md`.

En S7-1200 el temporizador **TON no es un tag Bool** que pones en un contacto.
Es un **bloque** que necesita su propia “cajita de memoria” (instance DB).

Por eso:
- en los `???` de arriba solo te aparece cosas como `DatosEstacion` → **no elijas eso**
- en un contacto normalmente abierto **no te deja** poner `T_EmpujePiston1` solo → es normal

---

## Lo que vas a crear

Temporizadores IEC de empuje (uno por pistón):

- `T_EmpujePiston1` / `T_EmpujePiston2` / `T_EmpujePiston3` — tipico `PT:=T#1s`

1. `T_RetardoPiston` — 0.5 s con el pistón extendido  
2. `T_TimeoutPiston` — 3 s de seguridad  

Cada uno genera un **Data block de instancia** automáticamente (ej. `T_RetardoPiston_DB` o similar). Eso está bien. **No uses el DB `DatosEstacion` para el TON.**

Si antes creaste un tag `%T0 Timer` en la tag table, puedes **ignorarlo / borrarlo**. En S7-1200 se trabaja con TON IEC.

---

## Paso a paso: insertar el TON (retardo pistón)

### 1) Red LAD
En `FC_Secuencia`, crea una network nueva.

### 2) Contactos a la izquierda (la condición IN)
Inserta contactos normalmente abiertos en serie:

- `M_Clasificando`
- `I_PistonExtendido`

(Eso alimenta la entrada del timer.)

### 3) Caja TON
1. Inserta **Empty box** (o arrastra TON desde Instructions → Basic instructions → Timer operations → **TON**).
2. En el centro de la caja escribe / elige **TON**.

### 4) El `???` de ARRIBA del bloque (lo más importante)
Ese campo es el **nombre de la instancia del timer**, no un tag de `DatosEstacion`.

1. Click en el `???` de arriba.
2. Escribe un nombre nuevo, por ejemplo: `T_RetardoPiston`
3. Acepta / Enter.
4. TIA te va a pedir crear un **data block** (instance DB) → dile que **sí** / OK.

Si te abre un selector y solo ves `DatosEstacion`:
- **no lo selecciones**
- busca botón tipo **Define / New / Create** o escribe el nombre nuevo a mano y deja que cree el DB
- a veces hay un icono de “nueva variable” o te deja tipear `T_RetardoPiston` directamente en el `???`

Resultado esperado: arriba del TON debe verse algo como `T_RetardoPiston` (y en el árbol de bloques aparece un DB de instancia).

### 5) Entrada `IN`
Conecta el riel de contactos a la entrada **IN** del TON  
(o pon en IN el último contacto de la serie).

### 6) Entrada `PT` (tiempo)
En el `???` de **PT**:
- escribe `T#500ms`  (0.5 segundos)  
  o `T#0.5s`

Tipo: TIME. No elijas `DatosEstacion` ahí.

### 7) Salida `Q`
La salida **Q** del TON es el “timer listo”.

Opciones válidas:

**Opción A (recomendada para empezar):**  
De la salida `Q` del TON saca un cable a una bobina Set/Reset o a la lógica de conteo.

**Opción B:**  
En **otra network**, usa un contacto normalmente abierto y como operand pon:

```text
T_RetardoPiston.Q
```

No pongas solo `T_RetardoPiston`.  
Tiene que ser **`.Q`** (la salida Bool del timer).

---

## Network de ejemplo (retardo pistón)

```
--| M_ClasifPlastico |----[ TON  T_EmpujePiston1  PT:=T#1s ]
--| T_EmpujePiston1.Q |----( R  M_ClasifPlastico )  // + contar

                                                                      Q ----( R  M_Clasificando )
```

Y en un network SCL / ADD (cuando `T_RetardoPiston.Q` sea true una vez) sumas aluminio.  
Para no contar muchas veces por ciclo, lo más simple es:

**Cuando Q del timer se active → Reset `M_Clasificando` + sumar 1 aluminio**  
(así al resetear se corta IN del TON y no reacciona otra vez).

Ejemplo práctico en 2 networks:

### Network A — TON
- Contactos: `M_Clasificando` + `I_PistonExtendido` → IN del TON `T_RetardoPiston`, PT=`T#500ms`

### Network B — Acción al cumplir tiempo
```
--| T_RetardoPiston.Q |----( R  M_Clasificando )
```
Y en la misma condición (o network SCL):
- `ContAluminio := ContAluminio + 1`
- sumar peso
- `UltimoMaterial := 2`

---

## Timeout pistón (alarma)

1. Otro TON, instancia nueva: `T_TimeoutPiston`
2. IN: solo `M_Clasificando`
3. PT: `T#3s`
4. Network aparte:

```
--| T_TimeoutPiston.Q |----|/| I_PistonExtendido |----( S  M_Alarma )
```

---

## Errores típicos

| Qué pasa | Causa | Qué hacer |
|---|---|---|
| En `???` solo sale `DatosEstacion` | Estás eligiendo un DB global viejo | Crea instancia nueva escribiendo `T_RetardoPiston` arriba del TON |
| Contacto no acepta el timer | El timer no es Bool | Usa `T_RetardoPiston.Q` |
| Creaste `%T0` en tag table | Estilo viejo S7-300 | Ignóralo; usa TON IEC |
| PT en rojo | Formato malo | Usa `T#500ms` o `T#3s` |

---

## Checklist rápido

- [ ] TON insertado como caja (no como tag en contacto)
- [ ] Arriba del TON: instancia nueva `T_RetardoPiston` (DB creado solo)
- [ ] PT = `T#500ms`
- [ ] Para preguntar “¿ya cumplió?” usas `T_RetardoPiston.Q`
- [ ] Lo mismo para `T_TimeoutPiston` con `T#3s`
