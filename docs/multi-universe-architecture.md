# CoreRPG — Regla de arquitectura multiuniverso

**Estado:** normativa arquitectónica  
**Fecha de adopción:** 2026-10-04  
**Proyecto:** `robdor80/CoreRPG`  
**Consumidores de referencia actuales:** Nimroel y Star Trek

## 1. Regla principal

CoreRPG es el **motor lógico común** de los videojuegos RPG del proyecto.

Debe permanecer:

- autoritativo;
- portable;
- determinista;
- reutilizable;
- independiente del universo;
- independiente de la presentación.

Nimroel, Star Trek y futuros juegos deben consumir el mismo núcleo.

> **Un solo motor. Múltiples universos.**

> **Si para añadir un nuevo juego hay que introducir en CoreRPG nombres o reglas
> propias de ese universo, la separación ha fallado.**

## 2. Separación de responsabilidades

```text
CONTENT PACK
    define contenido, posibilidades y reglas declarativas
        ↓
CoreRPG
    mantiene la realidad autoritativa
    valida solicitudes
    aplica reglas
    acepta o rechaza efectos
    conserva WorldState
        ↓
HOST
    presenta el juego
    recoge interacción
    integra plataforma, UI, audio, gráficos, red e IA
```

### Content Pack

Describe qué existe o puede existir en un universo mediante contratos
declarativos versionados.

No ejecuta código arbitrario ni es el motor.

### CoreRPG

Es propietario de la realidad viva y sus invariantes.

No conoce canon de Nimroel, Star Trek ni de futuros universos.

### Host

Presenta e integra.

Puede utilizar Unreal, consola, móvil u otra tecnología, pero no obtiene acceso
mutable al estado para saltarse las reglas del Core.

## 3. Clasificación obligatoria de nuevas necesidades

Toda nueva mecánica o capacidad debe pasar por esta decisión:

```text
¿Es específica de un universo?
        │
        ├── SÍ
        │    ↓
        │  mecánica específica del juego
        │
        └── NO
             │
             ├── ¿Es fundamental para la autoridad,
             │   el estado o la simulación compartida
             │   y existe una necesidad real?
             │        │
             │        ├── SÍ → CoreRPG
             │        │
             │        └── NO
             │             │
             │             ├── ¿dos o más consumidores comparten
             │             │   invariantes equivalentes?
             │             │       ├── SÍ → sistema reutilizable
             │             │       └── NO → mantener fuera hasta
             │             │                tener evidencia
             │             │
             │             └── no generalizar especulativamente
```

Ser independiente del universo **no basta** por sí solo para pertenecer al
núcleo.

## 4. Conceptos prohibidos como dominio duro del Core

Ejemplos Star Trek:

- WarpDrive;
- PhotonTorpedo;
- Klingon;
- Starfleet;
- LCARS;
- SensorArray;
- SubspaceScan;
- Transporter.

Ejemplos Nimroel:

- Dragon;
- Berserker;
- Treskal;
- Norgard;
- Mana;
- HouseAethros;
- Shäraldeen.

Si alguno de esos conceptos necesita comportamiento, debe expresarse sobre
capacidades generales o quedar en una extensión específica.

## 5. Conceptos candidatos genéricos

Según aparezcan necesidades reales, CoreRPG puede albergar o sustentar
abstracciones como:

- Entity;
- Definition;
- Capability;
- Action;
- Rule;
- Effect;
- Event;
- Observation;
- Knowledge;
- Relationship;
- Location;
- Time;
- Persistence;
- RNG;
- Simulation.

Otros conceptos aparentemente genéricos —inventario complejo, daño, crafting,
reputación, profesiones, habilidades, tareas, economía o combate— **no quedan
prometidos como parte del núcleo**. Su nivel se decidirá cuando existan
invariantes y consumidores suficientes.

## 6. Regla de generalización

No generalizar porque algo "parezca reutilizable".

Generalizar cuando consumidores reales demuestren que comparten el mismo
problema y las mismas invariantes relevantes.

Proceso recomendado:

```text
1. Aparece una necesidad real.
2. Se identifica el problema subyacente.
3. Se compara entre consumidores.
4. Se clasifica: Core / reusable / específico.
5. Se define el contrato.
6. Se implementa.
7. Se valida con tests neutrales.
8. Se valida con consumidores reales.
```

## 7. Nimroel y Star Trek como consumidores de referencia

Nimroel no dirige en exclusiva el diseño del Core.

Star Trek tampoco.

Cuando Nimroel necesite una capacidad se preguntará:

> ¿Cuál es el problema genérico que existe debajo?

Cuando Star Trek necesite una capacidad se hará la misma pregunta.

Después se comprobará cómo encaja en el otro consumidor.

Ejemplo:

```text
Nimroel
realidad -> percepción limitada -> observación -> conocimiento

Star Trek
realidad -> sensores limitados -> observación -> conocimiento
```

La mecánica superficial cambia; el problema estructural puede ser común.

De esa comparación pueden emerger abstracciones como:

- Observation;
- Knowledge;
- Confidence;
- Uncertainty;
- Detection;
- Tracking;
- Interference;
- Memory;
- Provenance;
- Information sharing.

No se introducirán hasta que sus contratos estén justificados por las fases que
los necesiten.

## 8. Mecánicas específicas y frontera de extensión

CoreRPG necesitará una frontera para mecánicas específicas de juego, pero la
tecnología queda deliberadamente pendiente.

Posibles mecanismos futuros, sin decisión actual:

- módulos enlazados en compilación;
- interfaces;
- registros controlados;
- capabilities;
- factories;
- scripting controlado;
- plugins;
- combinaciones de los anteriores.

No se implementará un sistema de plugins por anticipado.

### Invariante de autoridad

Una extensión específica puede:

- consultar estado mediante APIs controladas;
- evaluar reglas propias;
- producir resultados estructurados;
- proponer efectos.

No puede:

- mutar libremente el almacenamiento interno de `WorldState`;
- crear una autoridad paralela;
- saltarse validaciones del Core;
- hacer que el Host modifique la realidad directamente.

Flujo conceptual:

```text
Host
  ↓
solicitud estructurada
  ↓
mecánica genérica o específica
  ↓
resultado / efectos propuestos
  ↓
CoreRPG valida
  ↓
mutación autorizada
  ↓
WorldState
```

La forma técnica definitiva se decidirá cuando 4.5, 4.6, 4.10 y consumidores
reales aporten suficiente evidencia.

## 9. Content Packs

Los Content Packs son el mecanismo principal para describir universos.

A largo plazo podrán expresar, según contratos futuros:

- especies;
- culturas;
- personajes;
- lugares;
- facciones;
- objetos;
- profesiones;
- calendarios;
- economía;
- arquetipos;
- campañas;
- lore;
- contenido narrativo;
- capacidades;
- reglas configurables.

Esta lista describe **dirección de producto**, no las capacidades actuales del
Content Contract `0.1.0`.

Actualmente el contrato sigue limitado deliberadamente a identidad,
clasificación, referencias, manifests, dependencias y compatibilidad. Los
payloads de dominio se introducirán sólo cuando exista un modelo concreto y un
consumidor real.

## 10. No duplicación

Ningún repositorio de juego debe implementar un núcleo paralelo equivalente a
CoreRPG.

No crear un segundo:

- WorldState;
- RulesEngine;
- ActionEngine;
- SimulationCore;
- PersistenceCore.

Sí pueden existir:

- adaptadores;
- módulos específicos;
- presentación;
- integraciones;
- reglas de universo;
- sistemas exclusivos.

Todos ellos deben respetar la autoridad central del Core.

## 11. Validación obligatoria desde 4.5

Toda decisión arquitectónica relevante se revisará con estas preguntas:

1. ¿Contiene nombres o supuestos propios de Nimroel, Star Trek u otro universo?
2. ¿Funciona razonablemente para Nimroel y Star Trek?
3. ¿Se está generalizando antes de tener evidencia?
4. ¿Es dato, regla, núcleo genérico, sistema reusable o extensión específica?
5. ¿CoreRPG conserva la autoridad?
6. ¿El Host continúa limitado a presentación, integración e interacción?
7. ¿El Content Pack continúa declarativo?
8. ¿Se está creando accidentalmente un segundo motor?
9. ¿Una tercera ambientación hipotética revela una suposición oculta?

Esta revisión es obligatoria especialmente en:

- 4.5 — Generic World State;
- 4.6 — RNG + rules;
- 4.9 — Knowledge & Visibility;
- 4.10 — actions + simulation;
- 4.12 — Context Builder + AI;
- fases posteriores de Content Packs reales.

## 12. Criterio de éxito

La arquitectura estará correctamente encaminada cuando pueda demostrar algo
equivalente a:

```text
CoreRPG
   │
   ├── Content Pack Nimroel
   │       ↓
   │    mundo Nimroel
   │       ↓
   │    acciones Nimroel
   │
   └── Content Pack Star Trek
           ↓
        mundo Star Trek
           ↓
        acciones Star Trek
```

sin insertar canon de esos universos en el núcleo.

Podrán existir módulos específicos compilados o enlazados si la futura
arquitectura lo decide. La condición innegociable es que el **núcleo CoreRPG
permanezca el mismo**.

## 13. Regla de evolución

No construir arquitecturas gigantescas "por si acaso".

Tampoco resolver una necesidad real contaminando el Core.

CoreRPG crecerá mediante contratos pequeños, neutrales, testeables y
justificados por consumidores reales.

## 14. Frase de control

> **Un motor. Múltiples universos. La autoridad es del Core; el contenido
> describe; el Host presenta; las extensiones nunca se saltan las invariantes.**
