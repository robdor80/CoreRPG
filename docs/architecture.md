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
solicitan operaciones y consultas; no reciben acceso mutable al almacenamiento
interno para saltarse sus invariantes. Las futuras acciones y reglas validarán
y aplicarán cambios mediante APIs controladas del Core. Esta es una frontera
arquitectónica: todavía no existe un framework general de comandos o acciones.

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

Los Content Packs son inicialmente declarativos. Las diferencias entre
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

## Arquitectura multiuniverso

CoreRPG es un **único núcleo autoritativo, portable, determinista y reutilizable**
destinado a servir a universos y juegos distintos. Nimroel y Star Trek son desde
ahora los dos consumidores reales de referencia para validar sus abstracciones,
pero ninguno de los dos define por sí solo el dominio del Core.

La regla de control es:

> Si para añadir un nuevo juego hay que introducir en CoreRPG nombres, tipos o
> reglas propias de ese universo, la separación arquitectónica ha fallado.

El mismo núcleo debe poder sostener experiencias superficiales muy diferentes.
El Content Pack describe el universo y sus posibilidades lógicas; el Host
presenta la experiencia e integra plataforma y servicios externos; CoreRPG
mantiene el estado autoritativo, valida solicitudes, aplica reglas y acepta o
rechaza mutaciones.

### Tres niveles de responsabilidad

Una necesidad nueva no se incorpora automáticamente al Core sólo por parecer
genérica. Se clasifica mediante evidencia de consumidores reales:

1. **Núcleo genérico:** capacidades fundamentales para identidad, autoridad,
   estado vivo, determinismo, reglas, acciones, tiempo, conocimiento,
   persistencia o simulación que sean independientes de cualquier universo y
   necesarias para el motor común.
2. **Sistema RPG reutilizable:** una capacidad compartida por varios juegos con
   invariantes realmente equivalentes. No se crea especulativamente.
3. **Mecánica específica de juego:** comportamiento que sólo tiene sentido bajo
   el canon o las reglas de un universo concreto.

Por tanto, la pregunta no es sólo "¿esto es genérico?", sino también "¿es una
responsabilidad fundamental del motor común y existe una necesidad real?". Un
sistema como crafting, reputación o economía compleja puede ser agnóstico de
universo sin pertenecer necesariamente al núcleo.

### Prohibición de lógica por juego dentro del núcleo

CoreRPG no contendrá ramas del tipo:

```cpp
if (game == STAR_TREK) { /* ... */ }
if (game == NIMROEL)   { /* ... */ }
```

Tampoco utilizará como tipos fundamentales conceptos como `WarpDrive`,
`PhotonTorpedo`, `Klingon`, `Dragon`, `Treskal` o `Mana`. Las
necesidades concretas deben reducirse a contratos suficientemente generales o
permanecer fuera del núcleo.

### Mecánicas específicas y puntos de extensión

La existencia de varios consumidores demuestra que CoreRPG necesitará a medio
plazo una frontera formal para mecánicas específicas. Esto **no** autoriza a
implementar ahora scripting, plugins, registries o una ABI dinámica.

La tecnología de extensión se decidirá cuando exista un caso técnico real.
Hasta entonces la arquitectura debe conservar una propiedad: una mecánica
específica nunca obtiene autoridad paralela sobre el mundo.

Conceptualmente:

```text
Host / juego
    -> solicita una acción
    -> lógica específica calcula o propone un resultado
    -> CoreRPG valida contexto, reglas y efectos
    -> CoreRPG acepta o rechaza la mutación
    -> WorldState conserva el estado autoritativo
```

Una extensión no debe recibir acceso mutable libre al almacenamiento interno de
`WorldState` para saltarse invariantes. Podrá consultar mediante contratos
controlados y devolver resultados o efectos estructurados que el Core someta a
la misma autoridad que cualquier otra operación.

### Content Packs y mecánicas

Los Content Packs continúan siendo declarativos. Describen contenido,
posibilidades, referencias, capacidades y, cuando el contrato lo permita,
reglas configurables. No son motores ejecutables.

La lista conceptual de contenido futuro —especies, culturas, personajes,
lugares, naves, objetos, calendarios, economía, campañas o lore— no implica que
el Content Contract actual ya pueda representarlo. El contrato JSON `0.1.0`
sigue siendo deliberadamente un envelope mínimo de identidad, clasificación,
referencias y compatibilidad. Sus payloads de dominio se diseñarán sólo ante un
modelo y consumidores reales.

### Validación cruzada desde 4.5

Desde **4.5 — Generic World State**, toda decisión arquitectónica relevante debe
comprobarse al menos contra Nimroel y Star Trek:

- ¿contiene nombres o supuestos de un universo?
- ¿representa razonablemente las necesidades de ambos sin canon específico?
- ¿se está generalizando antes de tener invariantes compartidas?
- ¿pertenece al Core, a un sistema reutilizable, a datos declarativos o a una
  extensión específica?
- ¿CoreRPG conserva la autoridad sobre las mutaciones?
- ¿el Host sigue siendo presentación, integración e interacción?
- ¿el Content Pack sigue siendo declarativo?
- ¿algún repositorio de juego está creando un segundo motor?

Como prueba adicional de olor arquitectónico podrá utilizarse ocasionalmente un
tercer universo hipotético para detectar supuestos compartidos accidentalmente
por Nimroel y Star Trek.

### No duplicación del motor

Los repositorios de juegos pueden contener adaptadores, presentación,
integraciones y mecánicas exclusivas. No deben implementar un segundo
`WorldState`, `RulesEngine`, `ActionEngine`, `SimulationCore` o
`PersistenceCore` que compita con CoreRPG.

La autoridad central permanece en CoreRPG aunque la experiencia final y las
mecánicas de superficie sean radicalmente distintas.

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

`DeterministicRandom` proporciona estado explícito, secuencia determinista,
`uniform_closed()`, restauración mediante `state()` y vectores dorados de test.
Esta es infraestructura mínima, no la fase funcional de RNG y reglas: ownership,
streams, política de semillas y consumo entre reglas permanecen para 4.6.

`RollAction` es una vertical mínima de foundation. Demuestra input estructurado,
validación contra `WorldState`, consumo determinista de RNG, resultado explícito
y fallo sin mutación persistente. No es el framework general de acciones: no
existen todavía una clase base `Action`, un sistema de comandos, dispatcher,
registry, efectos generales ni framework de simulación. Ese modelo corresponde
principalmente a 4.10.

## Identidad

El modelo distingue tres tipos de identidad no intercambiables:

- `EntityId` identifica una entidad viva dentro de un `WorldState`.
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
IDs emitidos no se reutilizan durante la vida de ese estado del mundo. No existe
eliminación de entidades actualmente.

`EntityId` es un wrapper fuerte sobre `std::uint64_t`. Su método `is_valid()`
indica únicamente que el valor es distinto de `0`; no demuestra que la entidad
exista en un mundo. La pertenencia se consulta mediante
`WorldState::contains()`. Un ID sólo tiene significado contextual respecto de
un `WorldState`: dos mundos independientes pueden emitir correctamente
`EntityId{1}` y esos valores no constituyen una identidad global.

`DefinitionId` y `EntityId` también separan contenido e instancia. El primero
identifica de forma estable una definición declarativa; el segundo, una entidad
viva en un mundo. Todavía no existe una relación general de instanciación
`DefinitionId` -> `EntityId`, ni provenance de instancia, spawning, templates o
archetypes. Esta ausencia es deliberada hasta que una fase posterior demuestre
qué contrato necesita.

## Estado vivo mínimo

El `WorldState` actual no es todavía el Generic World State completo. Mantiene
la generación monotónica de `EntityId`, reconoce como existentes los IDs ya
emitidos y expone `entity_count()` y `revision()`. No contiene registros de
entidad, componentes, propiedades, relaciones, localizaciones, inventarios,
lifecycle, eliminación ni provenance de definiciones.

`WorldState::revision()` es una versión técnica observable del estado
persistente. Un mundo nuevo comienza en `0`; cada `create_entity()` aceptado la
incrementa exactamente una vez. Una creación fallida, una consulta y la
`RollAction` actual no la modifican. No representa tiempo del mundo, tick,
turno, fecha ni calendario.

Las futuras extensiones conservarán el contrato de mutación: una operación
persistente aceptada incrementa la revisión exactamente una vez y una operación
rechazada preserva `WorldState`. Esto fija la frontera de autoridad sin afirmar
que ya exista el futuro sistema general de acciones.

Actualmente `entity_count()` coincide con el último ID emitido porque sólo
existe creación y no existe eliminación. Es una consecuencia del modelo mínimo,
no una invariante permanente; ambas magnitudes podrán separarse cuando una fase
posterior introduzca lifecycle.

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

`ContentCatalog` representa definiciones declarativas validadas e inmutables;
`WorldState` representa la realidad viva de una partida. Todavía no existe el
puente general entre ambos, y este contrato no intenta diseñarlo de forma
anticipada.

`ResourceKind` identifica una clase lógica de recurso mediante la gramática
`[a-z][a-z0-9_-]*`. Es un identificador canónico abierto, no una ruta, una
extensión ni un schema concreto, y nunca se deduce de `DefinitionId`. Distingue
la clase declarada y permite validar el tipo esperado de una referencia, pero
no implementa por sí mismo un tipo de dominio. CoreRPG no mantiene un registry
global de kinds o codecs; esa decisión sólo se reabrirá ante consumidores
concretos que demuestren necesitarlo.

`ResourceDefinition` es el sobre mínimo e inmutable de una definición: contiene
exclusivamente su `DefinitionId` y su `ResourceKind`, ambos poseídos por valor.
El catálogo puede validar identidad, ownership, clasificación y relaciones,
pero la definición no transporta propiedades de dominio. No existe todavía un
payload genérico o tipado; los payloads y schemas específicos quedan fuera de
este contrato.

`ContentCatalog` posee un conjunto inmutable de definiciones en memoria. La
identidad completa debe ser única incluso si dos entradas declaran clases distintas.
El catálogo acepta el mismo local path bajo `PackId` diferentes, normaliza el orden
de entrada mediante el orden byte a byte de `DefinitionId` y usa ese orden para
búsqueda binaria y recorrido determinista. No define políticas de override, merge,
prioridad ni last-wins.

Su API permite comprobar existencia, buscar por `DefinitionId`, consultar el
`ResourceKind` y enumerar todas las definiciones en orden canónico. No expone
payloads de dominio ni modelos tipados específicos. No se añaden índices o
queries auxiliares mientras no exista un consumidor que los necesite.

La construcción del catálogo recibe `ContentDefinitionInput` temporales. Cada uno
asocia una `ResourceDefinition` candidata con referencias obligatorias descritas por
un `DefinitionId` target y el `ResourceKind` esperado, sin modificar el sobre mínimo
de la definición publicada. Resolver una referencia significa comprobar en memoria
que el target existe y que su tipo coincide; no crea punteros persistentes.
Tampoco conserva un grafo resuelto dentro del catálogo. La resolución por ID
preserva la inmutabilidad, evita acoplar consumidores al almacenamiento y deja
una futura ampliación de metadatos de relación para cuando exista un caso real.

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

Una futura ampliación estructural de los documentos de contenido requerirá una
nueva versión explícita del Content Contract y sus schemas. El número y la
política concreta se decidirán junto con esa ampliación; no se derivan de la
versión del Core ni de la de un pack.

### Contenido tipado todavía no definido

El Content Contract `0.1.0` es un envelope de identidad, clasificación,
referencias y compatibilidad, no un modelo de dominio. La ausencia de
propiedades adicionales es deliberada: todavía no existe un primer dominio y
un consumidor cuyas invariantes permitan diseñar una interfaz correcta.

Por ello no se incorporan por ahora `data: {}`, property bags recursivas, un
DOM JSON dentro del Core, `std::any`, blobs opacos, un `variant` cerrado con
todos los tipos RPG, un registry global de kinds o codecs ni una jerarquía
virtual de payloads. Estas alternativas no quedan prohibidas para siempre;
sólo podrán reabrirse ante una necesidad concreta, evaluando entonces tipado,
ownership, validación, determinismo, interoperabilidad y evolución.

JSON, filesystem y `CoreRPGContentJson` forman la adaptación física;
`ContentPackInput` y `ContentCatalog` forman el modelo lógico genérico. Una
futura extensión se diseñará primero como modelo lógico y después como
representación JSON. El formato físico no determinará el dominio ni filtrará
jsoncons a la API de CoreRPG.

La representación física inicial de ese contrato utiliza JSON UTF-8 y JSON
Schema Draft 2020-12. Su formato de manifest, índice explícito de recursos,
localizadores, documentos de definición y fronteras de validación se especifica
en [`content-json-contract-0.1.0.md`](content-json-contract-0.1.0.md).

La lectura física de esa representación pertenece a la biblioteca opcional
`CoreRPGContentJson`. Su lector recibe bytes en memoria, rechaza BOM, comentarios,
UTF-8 inválido, JSON mal formado y claves duplicadas antes de conservar un
documento opaco. El límite de tamaño es una política explícita del llamador. El
documento mantiene el DOM y jsoncons como detalles privados para que capas
posteriores puedan reutilizar el parseo sin exponer un modelo JSON genérico.

Sobre ese documento opaco, `CoreRPGContentJson` aplica JSON Schema Draft 2020-12
mediante un `SchemaRegistry` local y cerrado. El consumidor selecciona un rol
conocido —manifest o definición de recurso—; la propiedad `$schema` del
documento sólo se valida como contenido y nunca decide qué schema cargar. Los
schemas se embeben durante el build directamente desde los archivos canónicos
versionados y sus `$ref` se resuelven exclusivamente contra ese conjunto, sin
red ni búsqueda arbitraria en el filesystem. Los schemas compilados se
inicializan una sola vez y se reutilizan.

Después de validar el rol correcto, la misma biblioteca convierte directamente
el DOM ya parseado a los tipos fuertes de CoreRPG. Cada decoder selecciona y
ejecuta internamente su schema antes de leer campos; no confía en una validación
previa del llamador. Las fábricas de `PackId`, `DefinitionId`, `ResourceKind`,
`SemanticVersion` y `VersionRequirement` siguen siendo la autoridad sobre sus
invariantes, por lo que la conversión puede fallar aunque la forma JSON sea
válida. Los fallos de schema y de conversión se conservan como categorías
diagnósticas excluyentes.

El manifest físico se decodifica como un `ContentManifest` lógico junto con sus
localizadores de recursos poseídos. Los localizadores conservan el orden de
autoría sólo como representación; ningún comportamiento depende de esa posición.
Una definición física se convierte directamente en `ContentDefinitionInput`,
sin resolver referencias ni construir un catálogo.

El loader físico de `CoreRPGContentJson` recibe la ruta de un directorio de pack,
canonicaliza esa raíz y busca exclusivamente `manifest.json`. El índice
`resources` decodificado es autoritativo: cada localizador se resuelve respecto
a la raíz efectiva, se canonicaliza y sólo se lee si continúa siendo un
descendiente de esa raíz. Esto bloquea traversal textual y escapes mediante
symlinks sin descubrir archivos por recorrido. Una raíz suministrada como
symlink está permitida; su destino canonicalizado pasa a ser la raíz efectiva.

Manifest y recursos se leen en binario con límites explícitos por documento
(16 MiB y 64 MiB por defecto, respectivamente). El loader conserva por separado
los errores de filesystem, lectura JSON, schema y mapping, y sólo publica un
`ContentPackInput` después de cargar correctamente todos los recursos. Los
localizadores cumplen entonces su función física y no se conservan en el modelo
lógico. Los archivos no listados se ignoran.

```text
raíces de pack explícitas
    -> carga física guiada por manifest.json y resources
    -> lectura JSON estricta
    -> JsonDocument
    -> JSON Schema Draft 2020-12 del rol
    -> mapping a tipos fuertes
    -> ContentPackInput[]
    -> ContentCatalog::create
    -> ContentCatalog validado e inmutable
```

Este target depende de `CoreRPG`, pero `CoreRPG` no enlaza ni expone su stack
JSON. Un Host que construya `ContentPackInput` por otros medios puede utilizar
sólo la biblioteca de dominio. El loader físico individual termina en
`ContentPackInput`: no llama `ContentCatalog::create`, no resuelve referencias
ni duplica las validaciones semánticas globales del Core.

La operación multi-pack de la integración recibe una lista explícita de raíces,
carga cada una mediante el mismo loader físico y entrega todos los
`ContentPackInput` a `ContentCatalog::create`. No descubre directorios, no
traduce `PackId` a paths y no intenta cargar dependencias ausentes. Un
`PackDependency` sólo tiene significado lógico dentro del conjunto que el
caller proporcionó.

```text
raíces de pack explícitas
    -> load_content_pack(root) por cada raíz
    -> ContentPackInput[]
    -> ContentCatalog::create
    -> catálogo validado e inmutable
```

La operación completa es atómica. Un fallo físico conserva la raíz concreta y
el resultado detallado del loader; un fallo global conserva directamente los
`ContentDiagnostic` del Core. El orden de raíces sólo determina cuál es el
primer fallo físico observado durante la iteración y no establece prioridad,
override, precedencia ni orden semántico de carga.

La selección de packs continúa perteneciendo al caller, que entrega raíces
explícitas. Permanecen fuera del alcance actual discovery automático, un
registry de packs instalados, traducción implícita de `PackId` a filesystem,
auto-load de dependencias, search paths, ZIP, HTTP, hot reload, overrides,
prioridades, patch packs, merge, last-wins, scripting y plugins ejecutables.
No se afirma que estas capacidades no puedan existir en el futuro; reabrirlas
requiere una necesidad concreta y una decisión arquitectónica explícita.

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
- GoogleTest y CTest para los tests automatizados.
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
- payloads y schemas específicos de dominio para Content Packs;
- persistencia y migraciones;
- ABI dinámica estable;
- scripting y plugins ejecutables;
- red, replicación y multithreading interno;
- integración concreta con Unreal o proveedores de IA.

Estas fronteras permiten implementar una primera vertical pequeña sin cerrar
prematuramente decisiones que todavía no cuentan con casos de uso reales.

## Frontera entre motor genérico y dominio

La evolución del contenido distingue tres niveles conceptuales:

1. **Motor genérico:** identidad, catálogo, compatibilidad, validación,
   `WorldState` y reglas autoritativas.
2. **Posible modelo RPG reutilizable:** tipos concretos sólo cuando exista
   evidencia de que varios juegos pueden compartir sus invariantes.
3. **Universos concretos:** canon, contenido y mecánicas específicas de cada
   juego, mantenidos fuera del núcleo de CoreRPG y expresados mediante Content
   Packs, Hosts o futuras extensiones delimitadas según corresponda.

El nombre CoreRPG no obliga al motor genérico a conocer conceptos como
`Region`, `Settlement`, `PoliticalEntity`, `Culture`, `Species`, `Character`,
`Starship`, `BridgeStation` o `Faction`; tampoco se decide todavía si alguno
pertenecerá a una capa RPG reutilizable.

Antes de diseñar modelos destinados a contenido real se estudiarán necesidades
representativas de consumidores reales. Nimroel y Star Trek son actualmente los
dos consumidores de referencia y se utilizarán de forma cruzada para descubrir
invariantes, no para copiar canon de ninguno dentro del Core ni de sus fixtures.
Cuando sea útil, una tercera ambientación hipotética servirá como prueba de olor
para detectar supuestos compartidos accidentalmente por ambos.

## Interpretación incremental del roadmap

La infraestructura mínima existente no completa por adelantado las fases
funcionales posteriores:

- **4.2 — content pack contract:** construyó sustancialmente la infraestructura
  genérica del Content Pack system: identidades y versiones fuertes, manifests,
  compatibilidad, dependencias, referencias, catálogo atómico, diagnósticos
  deterministas y la adaptación JSON física aislada.
- **4.3 — minimal universe-agnostic core:** cierra el contrato mínimo de
  identidad viva, estado, revisión, determinismo y fronteras de mutación.
- **4.4 — content pack system:** reconcilia y cierra el alcance del sistema ya
  construido sin repetir 4.2 ni inventar payloads o modelos de dominio.
- **4.5 — Generic World State:** comienza cuando `WorldState` adquiere registros
  vivos y desarrolla un estado genérico real más allá del contador mínimo
  actual. Bootstrap, spawning, templates, archetypes, provenance viva y el
  puente general `DefinitionId` -> `EntityId` no pertenecen a 4.4.
- **4.6 — RNG + rules:** define ownership, streams, política de semillas y
  reglas funcionales sobre la infraestructura determinista existente.
- **4.10 — actions + simulation:** introduce el modelo general de acciones y
  simulación del que `RollAction` es sólo una vertical inicial.

La frontera entre 4.4 y 4.5 queda así fijada: 4.4 termina en la definición y
validación de contenido declarativo; 4.5 empieza cuando ese contenido pueda
relacionarse con registros vivos de un Generic World State real.

El contenido tipado de dominio se introducirá cuando exista un primer modelo
concreto validado y un consumidor real. En ese momento se decidirá si encaja en
una fase existente o si exige un ajuste explícito del roadmap; no se reserva
ahora un número nuevo.
