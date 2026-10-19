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
interno para saltarse sus invariantes. Las acciones y reglas validan y aplican
cambios mediante APIs controladas del Core. ActionEngine 4.10 aporta
un ejecutor tipado mínimo; no existe un framework universal de gameplay.

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
Esta infraestructura conserva su contrato; 4.6 añadió ownership, streams y
política de semillas explícita.

`RollAction` es una vertical mínima de foundation. Demuestra input estructurado,
validación contra `WorldState`, consumo determinista de RNG, resultado explícito
y fallo sin mutación persistente. Por sí sola no es un framework general de acciones.
ActionEngine 4.10 la reutiliza dentro de una solicitud tipada y Simulation
ordena esas solicitudes; no se crean clase base Action, registry o efectos
universales de gameplay.

### 4.6 — RNG streams y reglas estructurales (fusionada)

El generador SplitMix64 y sus vectores dorados de la foundation **no cambian**.
`RandomStreams` es un propietario explícito, por contexto de simulación, de
secuencias independientes. Recibe master seed `uint64`, y asocia
`stream_id uint64` con un `DeterministicRandom` único que se abre cuando
se solicita por primera vez; el ID 0 es válido. La seed inicial se obtiene
aplicando una pasada SplitMix64 a
`master_seed XOR ((stream_id + 0xD1B54A32D192ED03) * 0x9E3779B97F4A7C15)`,
con todas las operaciones como unsigned de 64 bits. Esta composición y las
semillas doradas son política de compatibilidad, no una fórmula modificable
silenciosamente. El ID de stream lo asigna cada consumidor como constante
estable, nunca por hashing de implementación ni usando orden de inserción.

El pool no es global, no está almacenado en `WorldState`, y las operaciones
que usan RNG deben seguir recibiendo el stream explícitamente. Consultar
streams inexistentes no los crea. Insertar otros no altera el estado de una
secuencia existente. Copiar el pool produce estados independientes. La
persistencia exacta de cada stream y su restauración se implementan en 4.7.

`RuleCheck` y `evaluate_rule_checks` son el primer contrato de
**comprobaciones puras, no autorizaciones** sobre hechos reales ya observables:
existencia, contención **directa** y ausencia de contención. Una conjunción
ordenada retorna el primer chequeo no satisfecho; distingue entrada malformada,
referencia inexistente y resultado legítimamente falso. No asigna memoria,
no consume RNG y no muta `WorldState`; la revisión del mundo permanece intacta.
No representa relaciones jerárquicas, roles, permisos, consecuencias, estado
de misión ni una política universal de personajes. Un chequeo satisfecho por sí
solo **no autoriza una mutación**: validación/ejecución general corresponde a
4.10 y a las reglas específicas bajo la autoridad del Core.

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
viva en un mundo. 4.8 permite una asociación opcional e inmutable a DefinitionId
existente, sin aplicar propiedades o instanciar payloads. No hay spawning de
arquetipos ricos, templates o registro universal de componentes.

## Estado vivo mínimo

El `WorldState` actual no es todavía el Generic World State completo. Mantiene
la generación monotónica de `EntityId`, reconoce como existentes los IDs ya
emitidos y expone `entity_count()` y `revision()`. El Incremento 2 de 4.5 añade
su primera capacidad persistente mutable: contención directa, discreta y
opcional entre entidades runtime. No contiene registros universales de
entidad, componentes, propiedades, localizaciones métricas, inventarios,
lifecycle ni eliminación. 4.8 añade bindings opcionales a definiciones,
relaciones tipadas y reservas con origen de unidades emitidas, como se describe
en su sección específica.

El Incremento 1 de 4.5 refuerza estos fundamentos y añade
`next_entity_id(EntityId after)`. Devuelve por valor el menor ID existente
estrictamente superior al cursor, o `std::nullopt` si no hay otro. El cursor
`EntityId{0}` inicia el recorrido y no necesita pertenecer al mundo. El orden
es ascendente por `EntityId`, sin depender de memoria, hashes, RNG ni contenido.
La consulta no crea entidades ni modifica el contador o la revisión, no expone
almacenamiento interno y no materializa una lista completa.

Las consultas sucesivas no constituyen un snapshot: una creación intercalada
puede aparecer en la siguiente consulta, incluso después de haber observado un
fin de recorrido. No hay concurrencia ni locks; los snapshots poseídos de 4.7
se capturan en una llamada sin mutaciones concurrentes. La implementación
aprovecha el intervalo implícito actual, pero ese intervalo no forma parte del
contrato público de enumeración. Un cursor con el máximo valor de `uint64_t`
devuelve ausencia sin desbordar.

`WorldState::revision()` es una versión técnica observable del estado
persistente. Un mundo nuevo comienza en `0`; cada `create_entity()` aceptado la
incrementa exactamente una vez. Cada cambio de contención también la incrementa
una vez. Un rechazo, una no-op, una consulta y la `RollAction` actual no la
modifican. No representa tiempo del mundo, tick,
turno, fecha ni calendario.

Las futuras extensiones conservarán el contrato de mutación: una operación
persistente simple aceptada incrementa la revisión exactamente una vez y una
operación rechazada preserva `WorldState`. El lote 4.8 publica N creaciones
elementales atómicas y aumenta revisión N veces. Esto fija la frontera sin afirmar
que ya exista el futuro sistema general de acciones.

La creación comprueba por separado el agotamiento de IDs y de revisión antes
de modificar ninguno de los dos contadores. El máximo de `uint64_t` es un ID
válido emitible si todavía hay revisión disponible; la siguiente creación se
rechaza con `EntityId{0}` y conserva ambos contadores. Si la revisión ya es
máxima, se rechaza igualmente aunque queden IDs. No hay wraparound ni cambios
publicados sin revisión. `create_entity()` conserva `noexcept` y no asigna
memoria dinámica.

Desde el Incremento 2, la revisión puede avanzar sin emitir identidad y dejar
de coincidir con el último ID. El agotamiento de revisión se comprueba en toda
mutación persistente. Los tests ejercitan sus límites sobre las mismas
transiciones internas que usa producción, sin setters ni APIs de restauración
públicos; no construyen un `WorldState` público agotado.

Al cierre de Fase 4.5, el Incremento 2 incorporó contención; la
integración, auditoría final y cierre pertenecen al Incremento 3.

Actualmente `entity_count()` coincide con el último ID emitido porque sólo
existe creación y no existe eliminación. Es una consecuencia del modelo mínimo,
no una invariante permanente; ambas magnitudes podrán separarse cuando una fase
posterior introduzca lifecycle.

### Contención persistente opcional

La relación significa únicamente «esta entidad tiene este contenedor directo
registrado». Sólo vincula `EntityId` existentes en el mismo mundo. Cada entidad
puede tener cero o un contenedor directo; un contenedor puede tener varios
contenidos y también estar contenido. No se permiten autorreferencias ni ciclos.
Crear una entidad no crea ningún vínculo. Cambiar o retirar un vínculo conserva
todas las identidades y el conteo de entidades.

La participación es opcional. Ausencia de vínculo no significa inexistencia,
ubicación física desconocida ni ausencia del universo. Esta capacidad no
representa propiedad, posesión, custodia, residencia, parentesco, mando,
permisos, capacidad física, conocimiento, percepción ni un sistema espacial
completo. No usa `LocationId` ni `DefinitionId` y no conoce tipos de universo.

`container_of(entity)` devuelve un `ContainmentResult` por valor:

- `entity_not_found`: la entidad no existe en este mundo;
- `uncontained`: existe sin vínculo directo registrado;
- `contained`: existe y el campo `container` contiene su contenedor directo.

El campo `container` es inválido en los dos primeros casos. La consulta no
asigna memoria, no modifica estado ni consume RNG.

`place_entity(entity, container)` asocia o reasocia una entidad. Su resultado
`ContainmentChange` distingue `applied`, `no_change`, `entity_not_found`,
`container_not_found`, `self_containment`, `cycle_detected` y
`revision_exhausted`. La precedencia estable es: existencia de la entidad,
existencia del contenedor, autorreferencia, ciclo, no-op, revisión disponible,
preparación de memoria y publicación. Para detectar ciclos se sigue
iterativamente la cadena de ancestros del contenedor propuesto; si alcanza la
entidad, se rechaza. No hay recursión. Una petición del mismo contenedor es
`no_change`, incluso con revisión agotada. Entradas inválidas siguen devolviendo
su error antes de comprobar revisión.

`remove_containment(entity)` retira únicamente el vínculo directo de esa
entidad. Devuelve `applied` si lo había, `no_change` si la entidad existe sin
vínculo, `entity_not_found` si no existe y `revision_exhausted` si habría cambio
pero la revisión está agotada. Conserva los vínculos de sus contenidos y todas
las otras relaciones. No destruye ni borra entidades, no crea un destino
implícito y no significa muerte, desaparición, teleportación ni salida física
del universo. Esta operación no asigna memoria y es `noexcept`.

La única fuente de verdad es un `vector` privado de pares entidad-contenedor,
ordenado por entidad. No hay índice redundante de hijos ni registros vacíos
para entidades sin vínculo. La búsqueda usa orden numérico, no hashes o
direcciones. Una consulta directa cuesta O(log L); recorrer ancestros cuesta
O(H log L), y insertar o retirar cuesta O(L), para L vínculos y H ancestros.
No se promete esta representación como contrato permanente.

Todas las validaciones preceden a los cambios. La inserción de un nuevo par
utiliza la garantía fuerte de `vector` con elementos de movimiento y asignación
sin excepciones. Si la reserva falla, `std::bad_alloc` se propaga sin cambiar
IDs, vínculos ni revisión. La reasociación sustituye un ID sin asignar memoria;
la retirada desplaza elementos sin excepciones. Sólo después de publicar el
cambio se incrementa la revisión, cuya disponibilidad ya se ha comprobado.
No se necesita rollback ni un sistema de transacciones. `place_entity()` no
es `noexcept` ni convierte fallos de memoria en resultados semánticos.

`WorldState` conserva semántica de valor. Copiar conserva contadores y vínculos,
y las mutaciones de la copia son independientes. La asignación por copia
construye primero un valor completo y lo reemplaza mediante swaps sin
excepciones: un fallo de memoria conserva también el mundo destino completo.
Copiar o reemplazar un mundo no es una acción de gameplay y no incrementa por
sí mismo la revisión. Las consultas `constexpr` del Incremento 1 se mantienen
en C++20; los tests comprueban su evaluación en un mundo vacío creado y
destruido dentro de la expresión constante. En MSVC Debug, los proxies de
iteradores de `vector` pueden asignar memoria incluso en un mundo vacío;
no se promete que un objeto `constexpr WorldState` persistente pueda retener
esas asignaciones después de la evaluación constante.

El conjunto de vínculos se observa canónicamente recorriendo
`next_entity_id()` desde cero y consultando `container_of()` para cada entidad,
conservando los resultados `contained`. No se añade una segunda API de
enumeración. Las consultas sucesivas no son snapshots ante mutaciones
intercaladas y no hay soporte de concurrencia.

Los tests cubren las transiciones de revisión máxima y los errores mediante
las mismas funciones internas usadas por los métodos públicos. Un ejecutable
de tests aislado provoca fallos acotados de asignación ordinaria en asociación,
inserción con vínculos existentes y copia; también comprueba que reasociar y
retirar no asignan memoria. No hay allocators o hooks de testing en producción.
Esto no simula todos los fallos de memoria o plataformas posibles.

Estas operaciones son primitivas estructurales bajo autoridad de `WorldState`.
No autorizan por sí solas cambios de gameplay desde Hosts o IA. La autorización
contextual, permisos, capacidades, costes y reglas de gameplay siguen
requiriendo políticas de consumidores. ActionEngine añade ejecución estructural
con precondiciones y revisión, sin convertirla en autorización social. No se expone
almacenamiento mutable ni un setter universal.

La contención no depende de RNG ni de `ContentCatalog`. Las fases 4.6 y 4.7
consumen estas primitivas y observan los vínculos en orden canónico.
La persistencia de 4.7 restaura vínculos y revisión exactamente. El incremento
histórico de 4.5 no implementó formato de guardado, restore, puente
`DefinitionId`/`EntityId`, spawning, materialización, reglas de movimiento,
acciones generales ni simulación.

### Integración y fronteras posteriores

El Incremento 3 histórico de 4.5 verifica los fundamentos y la contención como conjunto, sin
añadir capacidades. Sus pruebas combinan creación, cadena anidada, reasociación,
rechazo de ciclos sobre la topología vigente, retirada con descendientes,
no-op, copia y asignación de varios niveles, y emisión independiente de IDs
posteriores. El estado lógico se observa exclusivamente mediante la API
pública. Una prueba adicional comprueba que `RollAction` produce la misma
secuencia y estado RNG con o sin contención, sin alterar los hechos del mundo.
Estas pruebas no implementan serialización ni un sistema de snapshots.

El alcance ejecutable de 4.5 es identidad runtime y vínculos persistentes de
contención. «Persistente» aquí significa que el hecho pertenece al estado vivo
y permanece hasta una mutación; no significa que exista guardado en disco.
No hay un modelo general de componentes o datos de dominio, instanciación
desde Content Packs, spawning, templates, archetypes ni provenance de instancia.
La contención opcional no es un sistema completo de espacio o movimiento.
Fase 4.5 quedó integrada en `main` mediante la PR #3 y validada con CI verde
Windows/MSVC y Linux/GCC (run 38365231770); el cierre se registra en
`docs/phases/4.5-generic-world-state.md`. Esto no añade serialización ni dominio.

Las fronteras futuras son:

- **4.6 — RNG + Rules:** consultar contención no consume RNG ni ejecuta reglas.
  Las reglas específicas permanecen fuera de estas primitivas estructurales.
- **4.7 — Persistence:** preserva identidades, estado del asignador,
  revisión y vínculos. La carga reconstruye el estado completo sin repetir
  mutaciones ni incrementar la revisión por cada vínculo restaurado; las APIs
  y el formato se describen en la sección de persistencia siguiente.
- **4.8:** la revisión multiuniverso distingue reservas y relaciones genéricas
  de familias, organización específica y representación. Véase ADR-0003.
- **4.9 — Knowledge & Visibility:** los vínculos son realidad autoritativa,
  no conocimiento o percepción de un observador.
- **4.10 — Actions + Simulation:** la atomicidad actual es local a cada
  primitiva. Una secuencia de llamadas no constituye una transacción general;
  si una operación posterior falla, las anteriores conservan sus cambios.
  ActionEngine reutiliza esa atomicidad; Simulation informa el prefijo completado.
  Permisos sociales, costes y autorización de gameplay siguen en consumidores.
- **4.11 — Inspector:** las consultas permiten observar sin mutar, con los
  límites de las consultas sucesivas y ausencia de concurrencia ya documentados.
- **4.12 — Context Builder + AI:** una IA podrá proponer intenciones mediante
  la frontera de acciones 4.10; las consultas no le otorgan autoridad para
  modificar la realidad.

## 4.7 — Persistencia lógica y almacenamiento del Host

`Persistence` es la única frontera amiga de `WorldState` y `RandomStreams`
para capturar y restaurar su estado completo; no introduce setters de campos
ni acceso mutable al almacenamiento. `CampaignSnapshot` es un candidato
temporal poseído por valor, no otro mundo autoritativo. Restaurarlo revalida
todos sus datos, prepara ambos destinos y publica con swaps sin excepciones.
No usa operaciones ordinarias de creación/contención ni reproduce tiradas.
Un rechazo o excepción conserva ambos destinos, incluida la master seed,
streams existentes y referencias previas. Una carga válida reemplaza ambos y
anula referencias previas a streams.

El snapshot v1 comprende el intervalo exacto de IDs emitidos y su asignador,
revisión técnica, vínculos directos y pool RNG. Conserva los streams ausentes
como ausentes. No inventa campos de gameplay ni incorpora ContentCatalog,
datos declarativos, personajes, tiempos, eventos, provenance o estado del Host.
No hay puente de instanciación que guardar todavía.

El codec v1 produce bytes canónicos con uint32/uint64 little-endian, orden
estrictamente ascendente y CRC32C. Ver [especificación normativa](persistence-format-v1.md)
y [auditoría, APIs y verificaciones](phases/4.7-persistence.md). Versiones y
políticas desconocidas se rechazan; no hay migraciones implícitas. La validación
es iterativa, O(L log L + S), con O(L) memoria auxiliar para L vínculos y S
streams, sin recorrer el intervalo entero de entidades. No se serializa layout
nativo, memoria, direcciones ni tipos de presentación. No se añaden dependencias.

El Core opera sobre bytes en memoria. Abrir archivos en modo binario, comprobar
errores de lectura/escritura, límites de entrada antes de leer, escritura
temporal, reemplazo duradero, backups y autenticación pertenecen al Host. CRC32C
detecta corrupción accidental y no autentica datos manipulados. El Host
coordina un punto sin mutaciones concurrentes: la atomicidad de carga no
constituye sincronización entre hilos ni una transacción de filesystem.

La Fase 4.7 está integrada mediante PR #5, commit `483ad13`, con CI de main
superada en [38381606241](https://github.com/robdor80/CoreRPG/actions/runs/38381606241).

## 4.8 — Entidades, reservas y relaciones

[ADR-0003](adr/0003-generic-population-relations.md) clasifica explícitamente las
necesidades. WorldState conserva una sola autoridad: añade tablas privadas
ordenadas para bindings a definiciones, triples dirigidos, reservas y orígenes.
No existe un ECS, property bag, familia universal o modelo humano de población.
Sus APIs y evidencias están en [Fase 4.8](phases/4.8-entities-population.md).

El binding es opcional, valida existencia en ContentCatalog y es inmutable;
repetirlo es no-op y otro ID se rechaza. La tabla de cada mundo es independiente;
copias pueden compartir valores de DefinitionId inmutables, sin compartir
mutaciones. No se copian ni generan payloads. Las operaciones siguientes siguen
requiriendo una futura política de gameplay bajo la autoridad del Core.

EntityRelation es (source, RelationKind, target), con tipo uint64 estable no
nulo y extremos existentes. Los triples son únicos y se consultan en orden
source/kind/target. Ciclos, self edges y varios tipos entre el mismo par son
válidos; no implican simetría, mando o parentesco. Contención conserva sus
propias restricciones y no se usa como pertenencia social.

Una reserva se declara en una entidad existente y conserva total original y
cantidad latente. Las unidades latentes no tienen identidad todavía. Un lote
acotado emite N EntityId contiguos, registra un origen inmutable y reduce latent
por N. Conservación: total = latent + número de orígenes. El origen describe la
reserva que individualizó, no pertenencia organizativa actual. La declaración
original repetida no repone unidades ni recrea individuos.

Materialize publica N creaciones elementales atómicamente y aumenta revisión
N veces; las mutaciones simples siguen aumentándola una vez. Todos los límites
se comprueban antes de reservar memoria; después se añaden registros escalares
sin excepciones y se publican contadores. No hay consumo RNG ni generación
de datos no definidos. Copias y fallos preservan el estado completo.

El Host decide qué IDs representar, sin un flag visual en el Core. Descargar
una escena no desindividualiza ni destruye. No hay actualización permanente,
agenda o simulación por individuo. El coste agregado depende de reservas, no
del tamaño de su total latente ni del número de gráficos activos.

El [formato v2](persistence-format-v2.md) extiende los snapshots explícitamente.
V1 continúa leyendo y reexportando sus bytes; no acepta campos nuevos. Capture
selecciona el formato mínimo sin pérdida. V2 incluye bindings, relaciones,
reservas y orígenes, con conservación, orden e integridad validados. Restore
requiere catálogo si hay bindings y prepara ambos valores antes de publicar
todas las tablas y RNG. No se guarda ni carga contenido externo automáticamente.
La selección de versiones de packs y generación de arquetipos ricos queda aplazada.

4.8 se integró mediante PR #6, merge `d6e2c8f`. Actions/Simulation 4.10 se
describe en su sección propia. Familias, rangos, especies y ciudadanía son políticas
externas o candidatos reutilizables que requieren invariantes demostradas.

## Knowledge y Visibility — Fase 4.9

[ADR-0004](adr/0004-observer-knowledge-projections.md) distingue realidad,
recuerdos persistentes y visibilidad temporal. WorldState posee una tabla
dispersa ordenada por observador/FactKey. Cada observador es una entidad existente,
sin conocimiento automático de sí mismo o de otras entidades. El conjunto cerrado
de hechos comprende existencia, definición, contenedor, relación exacta, reserva
y origen de población; no añade propiedades ni un motor de percepción.

`learn_fact` registra el valor actual solicitado, incluida ausencia conocida.
Falta de registro significa desconocido. Los recuerdos no se refrescan por cambios
del mundo: una relación recordada puede haber dejado de existir. Referencias sólo
a entidades conocidas para ese observador; revocar existencia retira hechos
dependientes en una sola revisión. Actualizaciones/no-ops/rechazos mantienen las
garantías de revisiones y excepción fuerte, sin RNG. Copias comparten únicamente
texto inmutable, con tablas independientes. Restaurar clona texto del candidato
para impedir mutaciones por alias conservados por un caller privilegiado.

`Visibility::project` combina recuerdos con concesiones temporales explícitas y
produce `ObserverView`, un valor de sólo lectura sin acceso al mundo, observadores
ajenos, contadores globales, revisión global, snapshots ni RNG. Enumera únicamente
existencias autorizadas; toda búsqueda desconocida retorna nullptr. No distingue
ID oculto, inválido o inexistente. Una concesión de vínculo no concede sus extremos;
las existencias correspondientes también deben estar autorizadas. No se filtra la
realidad oculta al omitir una concesión inválida. No hay herencia social de permisos.

El Host/servicio autentica y fija el observador y las concesiones; no delega el
builder a clientes. Acceso directo a WorldState o Persistence es privilegiado.
No se promete aislamiento de código hostil dentro del proceso, ocultación de
tiempos/asignaciones o anonimización de IDs opacos. Las vistas ya entregadas no
se revocan retroactivamente. El [codec v3](persistence-format-v3.md) conserva
recuerdos y ausencia, con carga conjunta atómica de mundo/RNG y compatibilidad
exacta v1/v2. Proyecciones temporales no forman parte del guardado.

## Actions & Simulation — Fase 4.10

Contrato en [ADR-0005](adr/0005-actions-explicit-simulation.md), implementación y
validación en [acta 4.10](phases/4.10-actions-simulation.md). 4.9 está fusionada
por PR #7, merge `e7108e5`; CI posterior con cinco jobs correctos según el cierre
comunicado. La rama 4.10 parte de ese main verificado mediante fetch.

ActionRequest contiene actor/origen existente, una operación cerrada y tipada,
revisión esperada opcional y precondiciones RuleCheck ordenadas. Actor y sujeto
pueden diferir; el actor no concede permiso social ni autentica la solicitud.
El Host de confianza autentica, autoriza y convierte entradas externas. NPC,
jugador y futuras propuestas IA utilizan la misma API privilegiada.

ActionEngine::process valida límites, origen, revisión y condiciones vigentes,
y ejecuta exactamente una primitiva: contención, relación o tirada con stream
explícito. No introduce otro WorldState, datos de gameplay, callbacks ni DSL.
Accepted entrega una consecuencia tipada; no_change y rejected no entregan efectos.
Los diagnósticos y revisiones son internos y pueden revelar hechos ocultos:
no se convierten en eventos públicos ni se entregan crudos al observador.
Knowledge sólo cambia mediante sus APIs explícitas y Visibility mantiene sus
concesiones; mutar realidad no actualiza recuerdos ni revela extremos desconocidos.

Las primitivas anteriores conservan su contrato y son privilegiadas para
inicialización/compatibilidad. Una mutación efectiva aumenta revisión una vez;
un no-op o rechazo no la modifica. Tiradas aceptadas no cambian revisión del
mundo; singleton no crea stream. Rango inválido o rechazo no consume RNG.
Después de abrir un stream no se ejecuta ninguna operación que pueda lanzar;
los resultados son valores sin asignaciones. Excepción conserva mundo y RNG.
Revisión esperada protege contra cambios de mundo desde una propuesta anterior,
pero no identifica sesiones ni deduplica tiradas/transportes: responsabilidad del Host.

Simulation::step procesa el span de entrada secuencialmente contra ese mismo
mundo/pool. El caller proporciona almacenamiento de salida. Lote por encima del
presupuesto o salida insuficiente se rechazan antes de ejecutar; precondiciones
se limitan por acción. Un paso completado incluye rechazos/no-ops en su cantidad
procesada. Fallos de memoria/capacidad interrumpen antes de la acción fallida y
retornan el prefijo definitivo: continuar sólo con el sufijo. No hay rollback
ni repetición automática de acciones confirmadas, ni atomicidad de lote completo.
No se promete límite de duración de una primitiva o del rejection sampling RNG.

No se necesita tiempo lógico persistente, cola ni log interno: las consecuencias
confirmadas viven en WorldState/RandomStreams y v1/v2/v3 ya las guardan. Se puede
guardar al retornar de step, también ante interrupción. El Host conserva entradas
pendientes/cursor o logs externos cuando los necesite; no se restaura una cola
inexistente ni se reproducen solicitudes al cargar. La continuidad causal se
expresa por entradas explícitas y ordenadas, sin FPS, reloj del sistema o motor
visual. Los pasos no individualizan reservas latentes automáticamente.

Las tablas siguen teniendo sus costes lineales/logarítmicos existentes; process
no copia el mundo y step no reserva resultados. La unidad de presupuesto es la
solicitud y sus checks, no bytes del mundo, NPC deliberados o tiempo real.
No hay soporte concurrente. Tareas, órdenes, compromisos, delegación y generación
de intenciones del RFC permanecen aplazados tras probar el mínimo. 4.10 no inició
Inspector, Context Builder, red, IA o integración UE5/Godot/Android.

## Inspector — Fase 4.11

[ADR-0006](adr/0006-privileged-read-only-inspector.md) y
[contrato/ejemplo](phases/4.11-inspector.md). Inspector observa el WorldState
canónico desde herramientas técnicas privilegiadas; no toma decisiones ni concede
autorizaciones. Consultas noexcept sin asignar memoria: metadatos, entidades,
definiciones en catálogo explícito, contención, relaciones dirigidas/tipadas,
reservas, origen y recuerdos. Colecciones prestadas con offsets y límites 1–256.
El Host mantiene mundo/catálogo vivos, excluye mutaciones concurrentes durante
uso y descarta préstamos antes de mutar/reemplazar. Revisión esperada rechaza
páginas obsoletas dentro de una sesión; no detecta reemplazos con igual revisión.

El diagnóstico comparte las invariantes de Persistence mediante adaptadores
prestados, sin capturar todo el mundo. Primer fallo estructurado, scratch
limitado explícitamente y resultados inconclusos por falta de recursos. Recuerdo
desactualizado no equivale a corrupción. No se cambia persistencia ni simulación.
ActionResult ya es un valor técnico suficiente, conservado explícitamente por
el Host; no se añaden logs, replay ni persistencia de resultados.

**ObserverView sigue siendo la única vía autorizada hacia jugadores, NPC, diálogo
e IA.** Nunca entregar Inspector, informes o resultados privilegiados a UI jugable
o prompts. La frontera C++ presupone Host confiable; no aísla código hostil dentro
del proceso. Sensores, permisos institucionales y confidencias siguen en los juegos.
Context Builder 4.12, integración nativa y modelos de dominio siguen pendientes.

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
`WorldState` representa la realidad viva de una partida. El binding 4.8 sólo
conserva una referencia estable a definición validada. No aplica payloads ni
implementa instanciación general o bootstrap de universos.

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
- migraciones de guardados y almacenamiento físico específico del Host;
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
- **4.5 — Generic World State:** incorpora identidad runtime, enumeración
  determinista y contención persistente opcional, con mutación controlada,
  revisión, atomicidad local y copia independiente. No completa el modelo
  general de componentes, bootstrap, spawning, templates, archetypes,
  provenance viva ni el puente `DefinitionId` -> `EntityId`. Que estas
  capacidades estén fuera de 4.4 no implica que se implementen en 4.5.
- **4.6 — RNG + rules:** define ownership, streams, política de semillas y
  reglas funcionales sobre la infraestructura determinista existente.
- **4.7 — Persistence:** snapshots completos, codec binario versionado y
  restauración atómica de WorldState y RandomStreams; almacenamiento físico
  del Host y sistemas de gameplay futuros quedan fuera.
- **4.8 — Entidades/población/relaciones:** bindings, grafo tipado, reservas
  latentes, individualización acotada y persistencia v2; sin modelos de familia,
  profesiones o generación de arquetipos ricos.
- **4.9 — Knowledge & Visibility:** recuerdos independientes, proyecciones y v3.
- **4.10 — actions + simulation:** solicitudes tipadas y ejecución por pasos
  ordenados/acotados, reutilizando también RollAction; sin framework universal.

La frontera entre 4.4 y 4.5 queda así fijada: 4.4 termina en la definición y
validación de contenido declarativo; 4.5 desarrolla hechos runtime mutables
independientes del catálogo. No requiere ni implementa todavía la asociación
general entre definiciones y entidades vivas. Los documentos históricos de
4.2 a 4.4 describen su alcance al cierre; no se reescriben para atribuirles
esta delimitación incremental posterior.

El contenido tipado de dominio se introducirá cuando exista un primer modelo
concreto validado y un consumidor real. En ese momento se decidirá si encaja en
una fase existente o si exige un ajuste explícito del roadmap; no se reserva
ahora un número nuevo.
