# Arquitectura inicial de CoreRPG

## Propósito

CoreRPG es el núcleo lógico y autoritativo de un RPG genérico. Mantiene la
realidad persistente del mundo, valida solicitudes y aplica reglas sin asumir
cómo se presenta la experiencia ni qué universo contiene.

El diseño debe permitir que la misma biblioteca sea utilizada desde Windows,
Android, herramientas de consola, tests automatizados, Unreal Engine y otros
hosts compatibles.

## Fronteras del sistema

La arquitectura se divide conceptualmente en tres responsabilidades:

```text
Content Pack
     |
     | definiciones lógicas
     v
  CoreRPG <---- comandos y consultas ---- Host
     |                                      |
     +------- resultados y eventos ---------+
                                            |
                                      UI / Unreal / IA
```

### CoreRPG

CoreRPG es propietario del estado vivo y de sus invariantes. Entre sus futuras
responsabilidades se encuentran las entidades, acciones, reglas, relaciones,
tiempo, simulación, percepción, persistencia y generación procedural.

Sólo el Core puede aceptar una mutación del mundo. Los hosts y herramientas
interactúan mediante comandos y consultas; no reciben acceso para modificar el
estado directamente.

CoreRPG no contiene conceptos de un universo concreto y no depende de sistemas
de presentación, red o plataforma.

### Content Pack

Un Content Pack representa conceptualmente un universo completo. Puede reunir
dos clases de contenido:

1. **Contenido lógico portable:** culturas, especies, personajes, lugares,
   profesiones, facciones, objetos, calendarios, economía, arquetipos, reglas,
   campañas, lore y contenido narrativo. Esta es la parte que CoreRPG podrá
   consumir.
2. **Contenido de presentación:** texturas, modelos, audio, animaciones, mapas
   de un motor u otros assets. Esta parte pertenece al Host y no es interpretada
   por CoreRPG.

Cuando el contenido lógico necesite asociarse a una representación, utilizará
identificadores abstractos. Cada Host resolverá esos identificadores a sus
recursos concretos. El Core no conocerá formatos como `.uasset`.

Los Content Packs serán inicialmente declarativos. Las diferencias entre
universos deben expresarse mediante datos, reglas configurables y capacidades
del Core. Código nativo cargable, scripting, plugins ejecutables y hot reload de
código quedan fuera del alcance hasta que exista un caso real que los
justifique.

### Host

El Host presenta la experiencia y adapta el entorno externo a los contratos de
CoreRPG. Puede encargarse de UI, entrada, renderizado, audio, almacenamiento,
red, ciclo de vida de la aplicación e integración con un motor como Unreal.

Un adaptador futuro de Unreal traducirá sus tipos y operaciones a la API
portable. Ese adaptador pertenecerá al Host o a un proyecto de integración, no
al núcleo.

## Integración con IA

CoreRPG podrá incorporar en el futuro un Context Builder que produzca vistas
estructuradas y controladas del estado. Esto permitirá que un Host proporcione a
una IA únicamente la información que un personaje, narrador o sistema pueda
conocer.

CoreRPG no realizará:

- llamadas HTTP;
- autenticación o gestión de credenciales;
- selección de proveedores o endpoints;
- llamadas directas a OpenAI u otros servicios;
- reintentos o políticas de red.

Estas responsabilidades corresponden al Host. La salida de una IA se tratará
como una solicitud no confiable: deberá convertirse en un comando estructurado
y validarse mediante las mismas reglas que cualquier otra entrada. Una IA nunca
modificará el estado directamente.

## Determinismo

El determinismo es una propiedad arquitectónica, no una optimización posterior.
El objetivo es que el mismo estado inicial, las mismas entradas y la misma
semilla produzcan el mismo resultado.

El diseño y las pruebas deberán controlar explícitamente:

- algoritmo, semilla y estado del RNG;
- orden de las operaciones y de carga de contenido;
- reloj y tiempo externos;
- locale y representación textual;
- generación y persistencia de identificadores;
- iteración de contenedores no ordenados;
- cálculos críticos de punto flotante;
- orden de efectos en trabajo paralelo;
- cualquier entrada procedente de plataforma, red o Host.

No se persistirán resultados dependientes de implementaciones no especificadas,
como hashes de biblioteca, ni se asumirá que el orden del filesystem es
estable.

Las pruebas deben ser independientes entre sí: no usarán la hora real, no
dependerán del orden de ejecución ni compartirán estado mutable global. Las
pruebas que incorporen variación en el futuro utilizarán semillas explícitas.

La fuente aleatoria inicial de CoreRPG usa SplitMix64 con un estado de 64 bits.
La semilla se conserva literalmente, incluido el valor `0`, y todas las
operaciones dependen sólo de aritmética unsigned de ancho fijo. El mapeo a
rangos cerrados usa rejection sampling propio en vez de distribuciones de la
biblioteca estándar. Los vectores dorados del RNG son parte del contrato de
compatibilidad: cambiar el algoritmo o su secuencia requiere una decisión
explícita.

El RNG se pasa explícitamente a las operaciones que consumen aleatoriedad y se
mantiene separado de `WorldState`. Una entrada inválida o un rango con un único
valor no avanza el RNG. La primera `RollAction` sólo produce un resultado y no
modifica estado persistente, por lo que tampoco cambia la revisión del mundo.

## Identidad

El modelo distingue tres tipos de identidad no intercambiables:

- `EntityId` identifica una entidad viva dentro de una campaña.
- `PackId` identifica de forma estable un linaje de Content Pack.
- `DefinitionId` identifica una definición lógica aportada por un Content Pack.

Un `PackId` consta de al menos dos segmentos ASCII minúsculos separados por
`.`. Cada segmento sigue `[a-z][a-z0-9-]*`. Un `DefinitionId` combina el
`PackId`, el separador `:` y uno o más segmentos locales separados por `/`; cada
segmento local sigue `[a-z0-9][a-z0-9._-]*`. Los segmentos `.` y `..` no son
válidos.

Estas identidades son claves lógicas, no rutas de filesystem. No contienen
versiones, no dependen de locale y se comparan byte a byte en su representación
canónica. Un ID publicado no debe reutilizarse después para otro significado
semántico, aunque cambien nombres visibles o versiones del pack.

Dentro de cada `WorldState`, los IDs de entidades vivas se asignan desde `1` de
forma monotónica y determinista. El valor `0` representa un ID inválido y los
IDs emitidos no se reutilizan durante la vida de ese estado del mundo.

## Versiones independientes

CoreRPG mantendrá separados cuatro ejes de evolución:

- `core_version`: versión de la biblioteca y de su API pública;
- `content_contract_version`: versión del contrato que describe contenido
  lógico;
- `pack_version`: versión de un Content Pack concreto;
- `save_format_version`: versión del formato de estado persistido.

Una actualización del Core no implica necesariamente cambiar los otros tres
ejes. Cada contrato define su propia política de compatibilidad y los mecanismos
de migración sólo se introducirán cuando sean necesarios.

La fuente única de `core_version` es `project(CoreRPG VERSION ...)` en CMake.
`library_version()` expone esa versión como texto y `current_core_version()`
expone el mismo valor como `SemanticVersion`; ninguna de estas APIs mantiene
una segunda copia manual de la versión de la biblioteca.

Las versiones usan inicialmente el subconjunto estable `MAJOR.MINOR.PATCH`, con
tres enteros de 32 bits sin signo y sin prerelease ni build metadata. Los
requisitos entre packs son rangos estructurados con un mínimo inclusivo y un
máximo exclusivo opcional; un máximo menor o igual que el mínimo es inválido.

`PackId` identifica el linaje y no contiene la versión. `pack_version` identifica
una publicación de ese linaje, mientras `content_contract_version` identifica
el contrato lógico que interpreta sus datos y `minimum_core_version` expresa
una restricción adicional sobre CoreRPG. Ninguna sustituye a las demás.

El manifiesto lógico separa campos normativos —identidad, versiones y
dependencias— de `display_name` y `description`, que son metadatos informativos.
Estos metadatos no forman parte de la identidad ni intervienen en decisiones de
compatibilidad.

## Catálogo de contenido en memoria

`ResourceKind` identifica una clase lógica de recurso mediante la gramática
`[a-z][a-z0-9_-]*`. Es un identificador canónico, no una ruta, una extensión ni un
schema concreto, y nunca se deduce de `DefinitionId`.

`ResourceDefinition` es el sobre mínimo e inmutable de una definición: contiene
exclusivamente su `DefinitionId` y su `ResourceKind`, ambos poseídos por valor. Los
payloads y schemas específicos quedan fuera de este contrato.

`ContentCatalog` posee un conjunto inmutable de definiciones en memoria. La
identidad completa debe ser única incluso si dos entradas declaran clases distintas.
El catálogo acepta el mismo local path bajo `PackId` diferentes, normaliza el orden
de entrada mediante el orden byte a byte de `DefinitionId` y usa ese orden para
búsqueda binaria y recorrido determinista. No define políticas de override, merge,
prioridad ni last-wins.

La construcción del catálogo recibe `ContentDefinitionInput` temporales. Cada uno
asocia una `ResourceDefinition` candidata con referencias obligatorias descritas por
un `DefinitionId` target y el `ResourceKind` esperado, sin modificar el sobre mínimo
de la definición publicada. Resolver una referencia significa comprobar en memoria
que el target existe y que su tipo coincide; no crea punteros persistentes.

El staging es interno y la construcción tiene semántica todo-o-nada: sólo publica un
`ContentCatalog` completo cuando no existen IDs duplicados, referencias ausentes ni
tipos incompatibles. En caso contrario devuelve diagnósticos estructurados, poseídos
y ordenados canónicamente. El orden original de definiciones y referencias carece de
semántica.

La unidad candidata de construcción es `ContentPackInput`, que posee un
`ContentManifest` y sus definiciones. Cada construcción acepta como máximo una
versión concreta de cada `PackId`, y cada `DefinitionId` debe pertenecer al pack de
su manifest. Los manifests participan en la validación, pero no se conservan dentro
del catálogo publicado.

Las dependencias declaradas son obligatorias y directas. El pack requerido debe
estar presente y su `pack_version` debe satisfacer el `VersionRequirement`. Las
dependencias duplicadas y las dependencias hacia el propio pack son inválidas. Una
referencia a otro pack requiere que el pack de origen declare directamente esa
dependencia; una dependencia transitiva no concede visibilidad.

Los ciclos y autorreferencias entre definiciones son válidos para el resolver
genérico, pero los ciclos entre `PackDependency` invalidan la construcción. El
orden de packs, manifests, dependencias, definiciones y referencias no tiene
semántica y no establece load order, prioridad, override ni merge. El conjunto
completo conserva semántica todo-o-nada.

La política de compatibilidad es inmutable y mantiene separados `core_version`
y la versión de contrato soportada. CoreRPG soporta inicialmente exactamente el
Content Contract `0.1.0`: un manifest sólo es compatible cuando su
`content_contract_version` es igual a esa versión, sin rangos ni inferencias por
major o minor. Además, `minimum_core_version` debe ser menor o igual que
`current_core_version()`.

Las dos condiciones se validan independientemente y, cuando ambas fallan, se
devuelven ambos diagnósticos en orden determinista. Esta compatibilidad se
comprueba después de eliminar ambigüedades de identidad y ownership, pero antes
de interpretar dependencias o referencias. CoreRPG `0.1.0` y Content Contract
`0.1.0` coinciden actualmente de forma circunstancial: son ejes independientes
y ninguno se deriva del otro.

La representación física inicial de ese contrato utiliza JSON UTF-8 y JSON
Schema Draft 2020-12. Su formato de manifest, índice explícito de recursos,
localizadores, documentos de definición y fronteras de validación se especifica
en [`content-json-contract-0.1.0.md`](content-json-contract-0.1.0.md).

La futura lectura de esa representación pertenece a la biblioteca opcional
`CoreRPGContentJson`. Este target depende de `CoreRPG` y mantiene su stack JSON
como detalle privado. `CoreRPG` no enlaza ni expone esa dependencia, por lo que
un Host que construya `ContentPackInput` por otros medios puede utilizar sólo la
biblioteca de dominio. El target existe inicialmente como frontera de build; la
lectura estricta, la validación de schemas y la conversión a tipos fuertes se
incorporarán en incrementos posteriores.

La biblioteca seguirá Semantic Versioning. Antes de `1.0.0`, la API podrá
evolucionar mientras se descubre el dominio; alcanzar `1.0.0` requerirá una API
pública y una política de compatibilidad explícitas.

## Stack y políticas iniciales

- C++20.
- CMake como sistema de build.
- `CoreRPG` como biblioteca estática, sin promesa de ABI estable, y una capa
  opcional separada cuando sea necesario aislar dependencias de integración.
- Namespace raíz `corerpg`.
- Cero dependencias externas en `CoreRPG`; las integraciones opcionales aíslan
  sus dependencias en targets separados.
- GoogleTest y CTest cuando se introduzca el primer conjunto de tests.
- `clang-format` y adopción progresiva de `clang-tidy`.
- Compatibilidad prevista con MSVC, Clang y GCC.
- Semantic Versioning para las versiones publicadas del Core.

La biblioteca estática es un mecanismo inicial de distribución, no una
restricción permanente. Cualquier cambio se justificará por necesidades de
consumidores reales.

## Política de crecimiento

CoreRPG comenzará como una biblioteca única con áreas lógicas internas. No se
crearán módulos, factories, interfaces o sistemas de plugins de forma
preventiva. Una separación física exigirá una razón técnica observable, como
un ciclo de vida distinto, una dependencia aislable o varios consumidores con
necesidades diferentes.

Quedan expresamente pospuestos:

- sistemas completos de gameplay;
- loaders y formatos de Content Packs;
- persistencia y migraciones;
- ABI dinámica estable;
- scripting y plugins ejecutables;
- red, replicación y multithreading interno;
- integración concreta con Unreal o proveedores de IA.

Estas fronteras permiten implementar una primera vertical pequeña sin cerrar
prematuramente decisiones que todavía no cuentan con casos de uso reales.
