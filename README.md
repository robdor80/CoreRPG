# CoreRPG

CoreRPG es una biblioteca C++ para construir la lógica de juegos de rol de
forma portable, determinista y reutilizable. Su responsabilidad es representar
la realidad del mundo, validar acciones y conservar el estado autoritativo de
una campaña, sin depender de un universo concreto ni de una tecnología de
presentación.

## Qué es CoreRPG

CoreRPG pretende proporcionar progresivamente sistemas genéricos como
entidades, relaciones, inventario, tiempo, reglas, acciones, eventos,
simulación, percepción, persistencia y generación procedural.

El proyecto se apoya en tres piezas con responsabilidades diferentes:

- **CoreRPG** mantiene el estado, aplica las reglas y decide el resultado de las
  acciones.
- Un **Content Pack** describe de forma declarativa un universo y sus
  posibilidades lógicas.
- Un **Host** presenta el juego, recoge la interacción y solicita acciones o
  consultas al Core.

El principio fundamental es:

> El Content Pack define posibilidades. El Host solicita acciones y presenta
> resultados. CoreRPG valida, decide y conserva la realidad.

## Qué no es CoreRPG

CoreRPG no es un juego ni un universo narrativo. En particular:

- no es NimroelRPG ni un juego de Star Trek, ni contiene canon específico de ninguno;
- no depende de Unreal Engine ni conoce Blueprints o archivos `.uasset`;
- no contiene UI, renderizado, audio ni assets visuales;
- no integra directamente proveedores de IA ni realiza operaciones de red;
- no permite que un Host o una IA modifiquen el estado saltándose las reglas;
- no ejecuta código, scripts o plugins proporcionados por Content Packs.

Los hosts pueden utilizar Unreal Engine, una interfaz de consola, una aplicación
móvil u otra tecnología compatible. Esas decisiones no deben filtrarse al
núcleo portable.

## Objetivos

- Mantener una frontera clara entre lógica, contenido y presentación.
- Poder ejecutar y probar la lógica sin abrir un motor gráfico.
- Reproducir resultados a partir del mismo estado, entradas y semillas.
- Admitir Content Packs declarativos e intercambiables.
- Evolucionar de forma incremental sin arquitectura especulativa.
- Facilitar una integración futura con hosts C++, incluido Unreal Engine.

## Principios de diseño

- **Portabilidad:** C++20 y biblioteca estándar como base del código de
  producción.
- **Determinismo:** toda fuente de variación relevante debe ser explícita y
  controlable.
- **Autoridad:** las mutaciones del mundo pasan por operaciones validadas por el
  Core.
- **Agnosticismo:** ningún universo, motor gráfico o proveedor externo forma
  parte del dominio genérico.
- **Simplicidad:** una única biblioteca al inicio; los módulos se separarán sólo
  cuando exista una necesidad técnica real.
- **Contratos explícitos:** contenido, guardados e integraciones evolucionarán
  mediante contratos versionados e independientes.

## Estado actual

El proyecto se encuentra en su fase de cimentación técnica. Su núcleo vivo es
deliberadamente mínimo: `EntityId` representa identidad dentro de un
`WorldState`, que emite IDs monotónicos, reconoce y enumera los ya emitidos,
cuenta entidades y expone una revisión técnica. La Fase 4.5 añade contención
directa opcional entre entidades, con asociación, reasociación y retirada
controladas; no constituye un sistema espacial completo. Un ID distinto de
cero es válido como valor, pero su existencia siempre se consulta respecto de
un mundo concreto; los IDs no son globalmente únicos entre mundos.

`DeterministicRandom` y `RollAction` forman una primera vertical reproducible
de input estructurado, validación y resultado explícito. La Fase 4.6 incorporó
`RandomStreams` (streams separados con master seed explícita) y
`RuleCheck` (comprobaciones puras de existencia y contención). No constituyen
un motor general de acciones, permisos ni políticas de universo. El contrato lógico
declarativo de Content Packs aporta identidades estables, versiones,
compatibilidad, manifests, dependencias directas, definiciones y un
`ContentCatalog` inmutable cuya construcción realiza la validación semántica
global de forma atómica y determinista. Las definiciones declarativas y las
entidades vivas usan identidades diferentes. La asociación opcional de 4.8
conserva una referencia a DefinitionId existente; no instancia payloads ricos.

La biblioteca opcional `CoreRPGContentJson` implementa el Content Contract JSON
`0.1.0`: lectura JSON estricta, validación local mediante JSON Schema Draft
2020-12, conversión a los tipos fuertes del Core, carga de packs físicos y
construcción de catálogos desde raíces explícitas. `ContentCatalog::create()`
continúa siendo la autoridad semántica global. El target principal `CoreRPG`
permanece independiente de JSON y filesystem.

Este contrato es deliberadamente un envelope de identidad, clasificación,
referencias y compatibilidad. Una `ResourceDefinition` contiene esencialmente
su `DefinitionId` y su `ResourceKind`: el sistema puede validar ownership y
relaciones, pero todavía no transporta propiedades de dominio ni ofrece
payloads genéricos o tipados. Esa extensión se diseñará sólo cuando exista un
primer modelo concreto validado y un consumidor real.

La Fase 4.7 implementa `Persistence`: snapshots del mundo y de su pool RNG,
exportación binaria canónica versionada y restauración exacta y atómica. El
Core entrega y recibe bytes; el Host conserva el control del almacenamiento
físico. La [especificación v1](docs/persistence-format-v1.md) define tamaños,
orden, CRC32C, validación y compatibilidad. El target principal no añade JSON
ni filesystem. La fase quedó fusionada en `main` mediante PR #5, merge
`483ad13`, con CI Windows/MSVC y Linux/GCC superada.

La Fase 4.8 añade bindings opcionales a definiciones, relaciones dirigidas
tipadas, reservas poblacionales agregadas y materialización lógica en lotes
acotados. Las entidades emitidas conservan identidad y origen aunque el Host
descargue su representación. No se generan nombres, profesiones o biografías.
Familia, rango y autoridad permanecen fuera del núcleo. El formato
[v2](docs/persistence-format-v2.md) preserva los datos nuevos; v1 conserva su
lectura y sus bytes. Los bindings se resuelven contra un catálogo explícito al
cargar. 4.8 se integró mediante PR #6, merge `d6e2c8f`.

La Fase 4.9 añade conocimiento persistente por observador y proyecciones
filtradas. Conocer una entidad no revela su definición ni sus vínculos.
`WorldState::learn_fact` recuerda un hecho estructural concreto; `revoke_fact`
y `revoke_knowledge` retiran recuerdos. `Visibility::project` construye una
`ObserverView` separada sin revisión ni contadores globales, con concesiones
temporales explícitas del caller de confianza. Los cambios secretos no
refrescan recuerdos. El [formato v3](docs/persistence-format-v3.md) guarda
Knowledge; v1/v2 conservan sus bytes y cargan Knowledge vacío.
WorldState, Persistence y el builder son APIs privilegiadas: los clientes no
confiables reciben sólo ObserverView. [Contrato y límites 4.9](docs/phases/4.9-knowledge-visibility.md).

Todavía no hay un estado del mundo genérico completo, un sistema extensible de
reglas de gameplay, un ActionEngine general, simulación, percepción completa,
simulación temporal de población, gameplay ni Content Packs reales. Las fases 4.2–4.4 cerraron el
contenido declarativo y el Core mínimo sin inventar dominio; **4.5 quedó
fusionada y validada** y aporta los vínculos vivos estructurales. **4.6 está
fusionada en `main`** (PR #4, `8f1cbf7`, CI Windows/Linux superada), con ownership
de RNG y comprobaciones de reglas
estructurales puras; no proporciona todavía acciones generales. La instanciación
de payloads de arquetipos, la política institucional y la
simulación son asuntos futuros. 4.10 contempla Actions + Simulation.

## Compilación

Se necesita CMake 3.25 o posterior y un compilador compatible con C++20. Los
presets no fijan un generador ni contienen rutas locales, por lo que CMake puede
seleccionar el toolchain disponible en cada equipo.

Para configurar y compilar una build de desarrollo:

```text
cmake --preset dev
cmake --build --preset dev
```

Para ejecutar los tests tras compilar:

```text
ctest --preset dev
```

El preset `dev` descarga una versión fijada de GoogleTest únicamente cuando
`BUILD_TESTING` está habilitado. Los presets internos `dev` y `release` también
activan la biblioteca opcional `CoreRPGContentJson`, que obtiene una versión
fijada de jsoncons durante la configuración. Para consumidores externos esta
integración permanece desactivada por defecto, de modo que usar únicamente
`CoreRPG` no obtiene ni enlaza el stack JSON.

También existe el preset `release` para una build optimizada sin tests. Las
dependencias de terceros se registran en
[`docs/third-party-dependencies.md`](docs/third-party-dependencies.md).

Las fronteras y decisiones iniciales se describen con más detalle en
[`docs/architecture.md`](docs/architecture.md). La política multiuniverso
adoptada antes de la fase 4.5 se documenta de forma normativa en
[`docs/multi-universe-architecture.md`](docs/multi-universe-architecture.md) y
su decisión arquitectónica se registra en
[`docs/adr/0001-multi-universe-architecture.md`](docs/adr/0001-multi-universe-architecture.md).

Documentación de etapas: [cierre 4.5](docs/phases/4.5-generic-world-state.md) · [historia 4.6](docs/phases/4.6-rng-rules.md) · [4.7 Persistence](docs/phases/4.7-persistence.md) · [4.8 entidades/población](docs/phases/4.8-entities-population.md) · [roadmap](docs/roadmap.md).
