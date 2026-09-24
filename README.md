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

- no es NimroelRPG ni contiene conceptos de Nimroel;
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
`WorldState`, que actualmente sólo emite IDs monotónicos, reconoce los ya
emitidos, cuenta entidades y expone una revisión técnica. Un ID distinto de
cero es válido como valor, pero su existencia siempre se consulta respecto de
un mundo concreto; los IDs no son globalmente únicos entre mundos.

`DeterministicRandom` y `RollAction` forman una primera vertical reproducible
de input estructurado, validación y resultado explícito. Son infraestructura de
foundation, no los sistemas completos de reglas y acciones. El contrato lógico
declarativo de Content Packs aporta identidades estables, versiones,
compatibilidad, manifests, dependencias directas, definiciones y un
`ContentCatalog` inmutable cuya construcción realiza la validación semántica
global de forma atómica y determinista. Las definiciones declarativas y las
entidades vivas usan identidades diferentes y todavía no existe un puente
general de instanciación entre ambas.

La biblioteca opcional `CoreRPGContentJson` implementa el Content Contract JSON
`0.1.0`: lectura JSON estricta, validación local mediante JSON Schema Draft
2020-12, conversión a los tipos fuertes del Core, carga de packs físicos y
construcción de catálogos desde raíces explícitas. `ContentCatalog::create()`
continúa siendo la autoridad semántica global. El target principal `CoreRPG`
permanece independiente de JSON y filesystem.

Todavía no hay un estado del mundo genérico completo, sistemas funcionales de
reglas, acciones o simulación, persistencia, conocimiento, población, gameplay
ni Content Packs reales. La fase 4.3 cierra el contrato del núcleo mínimo; 4.4
extenderá el sistema de contenido sin repetir 4.2, 4.5 desarrollará el estado
vivo genérico, 4.6 abordará RNG y reglas funcionales y 4.10 el modelo general de
acciones y simulación.

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
[`docs/architecture.md`](docs/architecture.md).
