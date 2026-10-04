# ADR-0001 — CoreRPG como motor multiuniverso único

- **Estado:** aceptado
- **Fecha:** 2026-10-04
- **Decisión:** CoreRPG será el núcleo lógico compartido por Nimroel, Star Trek y
  futuros juegos RPG del proyecto.

## Contexto

CoreRPG nació ya como biblioteca C++ portable, determinista, reutilizable y
agnóstica de universo. Durante las primeras fases Nimroel fue su principal
consumidor de referencia.

El proyecto dispone ahora de un segundo consumidor real, Star Trek, con
necesidades de gameplay muy distintas. Esta diferencia confirma dos hechos:

1. el núcleo no puede evolucionar siguiendo exclusivamente los conceptos de
   Nimroel;
2. algunos problemas que parecen específicos de un universo esconden
   abstracciones comunes útiles para el motor.

Ejemplo:

```text
Nimroel:
realidad -> percepción limitada -> observación -> conocimiento

Star Trek:
realidad -> sensores limitados -> observación -> conocimiento
```

La fase 4.5 todavía no ha comenzado y `WorldState` sigue siendo mínimo. Los
sistemas funcionales de reglas, acciones, simulación, persistencia y puntos de
extensión aún no están definidos. Por ello esta decisión se toma antes de que
esas capas consoliden supuestos difíciles de revertir.

## Decisión

Se adopta la siguiente política:

1. CoreRPG es un único núcleo autoritativo compartido.
2. Nimroel y Star Trek son consumidores de referencia, no dominios del Core.
3. Los Content Packs describen universos de forma declarativa.
4. Los Hosts presentan, integran y solicitan operaciones.
5. Toda mutación persistente del mundo permanece bajo autoridad de CoreRPG.
6. Las capacidades fundamentales e independientes de universo pueden vivir en
   el Core cuando exista necesidad real.
7. Los sistemas sólo se generalizan como reutilizables cuando varios
   consumidores demuestren invariantes equivalentes.
8. Las mecánicas verdaderamente específicas permanecen fuera del núcleo y, en
   el futuro, usarán una frontera de extensión controlada.
9. No se elegirá todavía la tecnología de extensión.
10. Una extensión nunca obtendrá acceso mutable libre a `WorldState`; deberá
    operar mediante contratos controlados y resultados o efectos validados.
11. Ningún repositorio de juego implementará un segundo motor autoritativo
    paralelo a CoreRPG.
12. Desde 4.5 las decisiones arquitectónicas se validarán conceptualmente contra
    Nimroel y Star Trek y, cuando aporte valor, contra un tercer universo
    hipotético.

## Motivos

### Evitar contaminación de dominio

El Core no debe adquirir tipos como `Dragon`, `Mana`, `WarpDrive`,
`PhotonTorpedo`, `Klingon` o `Treskal`.

### Evitar fragmentación

Crear un motor distinto por juego duplicaría:

- estado;
- reglas;
- acciones;
- persistencia;
- determinismo;
- conocimiento;
- simulación.

Eso produciría divergencia y multiplicaría el coste de evolución y pruebas.

### Permitir experiencias distintas

Compartir núcleo no obliga a compartir gameplay superficial.

Nimroel puede centrarse en exploración, diálogo, política, supervivencia y
fantasía medieval; Star Trek puede centrarse en procedimientos, consolas,
sensores, cadena de mando y operaciones de nave.

La diferencia pertenece a datos, Hosts, sistemas reutilizables cuando proceda y
mecánicas específicas, no a una bifurcación del núcleo.

### Generalizar con evidencia

Tampoco se quiere construir un "motor universal" especulativo.

Una capacidad no entra en Core sólo porque sea técnicamente genérica. Debe
existir una necesidad real y encajar en la responsabilidad fundamental del
motor. De lo contrario se mantiene fuera o se promueve a sistema reusable sólo
cuando varios consumidores compartan sus invariantes.

## Consecuencias positivas

- CoreRPG tendrá una frontera de dominio más limpia.
- Nimroel y Star Trek sirven como pruebas cruzadas de abstracciones.
- Se reduce el riesgo de diseñar `NimroelEngine` o `StarTrekEngine`.
- Los sistemas comunes podrán probarse con fixtures neutrales.
- Las mecánicas específicas tendrán un lugar arquitectónico explícito.
- La futura integración de nuevos universos no exige introducir su canon en el
  Core.

## Costes y consecuencias negativas

- Algunas decisiones requerirán más análisis antes de implementar.
- Puede existir lógica específica fuera del núcleo que no sea reutilizable.
- Habrá que diseñar en el futuro una frontera de extensiones.
- No siempre será evidente si una capacidad pertenece al Core, a un módulo
  reusable o a un juego.
- La validación cruzada puede descubrir que una abstracción temprana debe
  simplificarse o dividirse.

Estos costes se aceptan porque son preferibles a contaminar o duplicar el motor.

## Alternativas rechazadas

### Motor específico para Nimroel y otro para Star Trek

Rechazada por duplicación de autoridad, reglas, estado, persistencia y
simulación.

### Introducir condiciones por juego en CoreRPG

Rechazada porque convierte el núcleo en una colección de excepciones:

```cpp
if (game == NIMROEL) { ... }
if (game == STAR_TREK) { ... }
```

### Generalizar todos los sistemas RPG desde el principio

Rechazada por especulativa. Conceptos como combate, reputación, crafting,
profesiones o economía sólo se compartirán cuando consumidores reales
demuestren invariantes compatibles.

### Implementar ahora un sistema de plugins

Rechazada por prematura. La necesidad de una frontera de extensión es real; su
tecnología todavía no.

## Implicaciones inmediatas

Antes de implementar 4.5:

- `docs/architecture.md` debe reflejar esta política;
- la fase 4.5 deberá diseñarse sin supuestos exclusivos de personajes,
  humanoides, fantasía medieval o naves;
- 4.6 y 4.10 no deberán cerrar la posibilidad de mecánicas específicas
  controladas;
- los tests de Core seguirán siendo neutrales;
- el Content Contract `0.1.0` no se ampliará sólo por esta decisión.

## Criterio de revisión futura

Esta ADR deberá revisarse si:

- aparece un consumidor cuyo modelo no pueda expresarse razonablemente con las
  fronteras actuales;
- la autoridad exclusiva del Core impide una necesidad legítima;
- la integración de mecánicas específicas exige una frontera formal;
- dos o más juegos demuestran que un sistema actualmente específico merece
  convertirse en reusable;
- CoreRPG se aproxima a una ABI pública estable o a 1.0.

## Regla final

> **CoreRPG permanece uno. Los universos cambian; el núcleo autoritativo no se
> fragmenta.**
