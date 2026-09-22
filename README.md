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

El proyecto se encuentra en su fase de cimentación técnica. Existe un esqueleto
mínimo de biblioteca estática C++20, pero todavía no hay sistemas RPG, contratos
de datos, dependencias de producción ni tests implementados.

## Compilación

Se necesita CMake 3.25 o posterior y un compilador compatible con C++20. Los
presets no fijan un generador ni contienen rutas locales, por lo que CMake puede
seleccionar el toolchain disponible en cada equipo.

Para configurar y compilar una build de desarrollo:

```text
cmake --preset dev
cmake --build --preset dev
```

También existe el preset `release` para una build optimizada sin tests. Aunque
`BUILD_TESTING` ya sigue la convención de CMake, el framework y los tests se
añadirán en un incremento posterior.

Las fronteras y decisiones iniciales se describen con más detalle en
[`docs/architecture.md`](docs/architecture.md).
