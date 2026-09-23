# Content Contract JSON 0.1.0

## Propósito y alcance

Este documento define la representación física JSON del Content Contract
`0.1.0` de CoreRPG. Establece cómo los documentos mantenidos por autores se
convierten en el modelo lógico C++ existente:

```text
bytes JSON
      -> lectura estricta
      -> JsonDocument
      -> validación del schema del rol
      -> conversión a tipos fuertes
      -> loader físico y ContentPackInput
      -> validación semántica global y ContentCatalog
```

El contrato físico no modifica las responsabilidades de `ContentManifest`,
`ContentDefinitionInput`, `ResourceDefinition`, `DefinitionReference`,
`PackDependency`, `VersionRequirement` ni `ContentCatalog`. Este documento fija
la representación que implementan los schemas, el mapper y el loader de
filesystem; no define payloads específicos.

## Formato canónico

JSON es la fuente canónica humana de los Content Packs. Los documentos utilizan
JSON Schema Draft 2020-12 y cumplen estas reglas:

- el texto está codificado como UTF-8 válido;
- los documentos canónicos no contienen byte order mark (BOM);
- se utiliza JSON estándar estricto y no se admiten comentarios;
- el whitespace no tiene semántica;
- el orden de las propiedades no tiene semántica;
- identificadores y localizadores físicos utilizan ASCII estricto;
- `display_name` y `description` pueden utilizar UTF-8 completo.

Un posible formato binario, paquete o caché compilada futuro será siempre un
artefacto derivado y no sustituirá al JSON como fuente canónica humana.

Content Contract `0.1.0` no introduce números JSON normativos. Identificadores
y versiones son strings. Cualquier número de dominio futuro deberá declarar
explícitamente su tipo, rango, unidad y reglas relevantes para el determinismo,
sin depender de una conversión implícita a `double`.

## `manifest.json`

Cada pack posee un `manifest.json` en su raíz con esta forma física:

```json
{
  "$schema": "https://robdor80.github.io/CoreRPG/schemas/content/0.1.0/content-pack-manifest.schema.json",
  "pack_id": "example.test",
  "pack_version": "1.0.0",
  "content_contract_version": "0.1.0",
  "minimum_core_version": "0.1.0",
  "dependencies": [],
  "resources": [],
  "display_name": "Example Test",
  "description": "Neutral content pack used to validate the contract."
}
```

Son obligatorios `pack_id`, `pack_version`, `content_contract_version`,
`minimum_core_version`, `dependencies`, `resources`, `display_name` y
`description`. `dependencies` y `resources` aparecen incluso cuando están
vacíos. `display_name` y `description` aparecen incluso cuando su valor es un
string vacío. No se utiliza `null` para representar ausencia.

Los campos lógicos se convierten a `ContentManifest`. `resources` y `$schema`
pertenecen exclusivamente a la representación física. El resultado físico
decodificado conserva el `ContentManifest` junto con una lista poseída de strings
`ResourceLocator`; no introduce los localizadores en el modelo lógico. `$schema`
se descarta después de validar el documento.

### Propiedad `$schema` en contenido

La propiedad `$schema` es opcional, pero se recomienda en el contenido
mantenido por el proyecto para facilitar validación y asistencia en editores.

Esta propiedad:

- no determina la compatibilidad del pack;
- no sustituye a `content_contract_version`;
- no autoriza acceso de red;
- debe coincidir con el schema esperado para el rol y la versión del documento
  cuando está presente.

La validación selecciona el schema mediante el rol explícito que proporciona el
consumidor (`content_pack_manifest` o `resource_definition`), nunca leyendo esta
propiedad. `$schema` es sólo contenido validable: cuando aparece, el schema raíz
seleccionado exige mediante `const` que coincida con su `$id` canónico. No se
descarga ni se resuelve ninguna URI indicada por un documento no confiable.

## Índice físico `resources`

`manifest.json` contiene en `resources` el índice explícito de documentos de
definición pertenecientes al pack:

```json
"resources": [
  "resources/things/a.json",
  "resources/things/b.json"
]
```

Este índice:

- pertenece a la capa física JSON y al loader;
- no pertenece a `ContentManifest` C++;
- no forma parte de la identidad;
- no aparece en saves;
- no permanece en `ContentCatalog`;
- sólo determina qué documentos debe leer el loader.

El decoder y el loader pueden conservar el orden de autoría del array, pero ese orden no
empareja documentos con definiciones ni establece prioridad, load order u otra
semántica. El loader abre cada `ResourceLocator` explícito.

El runtime carga exclusivamente los documentos listados. No descubre contenido
recorriendo directorios y no interpreta el orden del filesystem. Un documento
listado que no exista produce un error de carga. Un documento no listado no
forma parte del pack y no se carga.

Las herramientas de autoría o empaquetado podrán detectar JSON no listados bajo
el directorio reservado `resources/`, pero esa comprobación no convierte el
recorrido en un mecanismo de descubrimiento o de orden de carga.

En runtime el loader busca exclusivamente `manifest.json` en la raíz recibida;
no acepta nombres alternativos ni busca packs o manifests recursivamente. Lee
exactamente los recursos listados. README, licencias, metadata futura y JSON no
listados pueden coexistir en el directorio y se ignoran.

## `ResourceLocator`

Cada elemento de `resources` es un localizador físico relativo a la raíz del
pack. Su gramática canónica es:

```regex
^resources/(?:[a-z0-9][a-z0-9_-]*/)*[a-z0-9][a-z0-9_-]*\.json$
```

Un `ResourceLocator` contiene exclusivamente ASCII, utiliza minúsculas,
comienza por `resources/`, utiliza `/` como único separador, termina en `.json`
y se compara byte a byte. La comparación es case-sensitive incluso cuando el
filesystem subyacente no lo sea.

No son válidos:

- paths absolutos;
- backslashes;
- `:`;
- URLs o cualquier otro URI;
- segmentos vacíos;
- los segmentos `.` y `..`;
- percent encoding;
- caracteres Unicode;
- extensiones distintas de `.json`.

El loader deberá resolver el localizador sin permitir que el resultado escape
de la raíz del pack.

La raíz suministrada se canonicaliza. Si es un symlink, su destino se acepta
como raíz efectiva. Cada recurso existente también se canonicaliza y se compara
con esa raíz usando componentes e identidad de filesystem, nunca mediante un
prefijo textual. Un symlink intermedio o final que resuelva fuera del pack se
rechaza. La misma regla impide que `manifest.json` sea un symlink hacia fuera.

Esta protección se apoya en las operaciones portables de `std::filesystem` y
presupone que el árbol del pack no se modifica concurrentemente durante la
carga; la biblioteca estándar no ofrece una apertura race-free equivalente a
`openat`/`O_NOFOLLOW` en todas las plataformas. En filesystems Windows
case-insensitive, el schema mantiene los localizadores contractuales en
minúsculas, pero el loader no intenta verificar el casing físico almacenado del
nombre.

`ResourceLocator` no es `DefinitionId`. La ubicación física no define identidad,
ownership ni `ResourceKind`. Renombrar un archivo no cambia la identidad lógica
de la definición que contiene.

## Un archivo por definición

Content Contract `0.1.0` contiene exactamente una `ResourceDefinition` por
archivo JSON. La raíz de un documento de definición es un objeto, nunca un array
de definiciones.

Esta forma favorece diffs pequeños, reduce conflictos en Git, simplifica la
edición humana y proporciona una unidad clara para diagnósticos y tooling. El
contrato inicial no introduce una representación alternativa para optimizar un
hipotético número elevado de archivos.

## Documento de definición

El envelope físico inicial es:

```json
{
  "$schema": "https://robdor80.github.io/CoreRPG/schemas/content/0.1.0/resource-definition.schema.json",
  "id": "example.test:things/a",
  "kind": "thing",
  "references": []
}
```

`id`, `kind` y `references` son obligatorios. `$schema` es opcional con la misma
política definida para `manifest.json`. Una definición sin referencias conserva
explícitamente el array vacío.

`id` contiene siempre un `DefinitionId` completo y autocontenido. No existen IDs
relativos en esta versión. El loader no infiere el `PackId` desde el manifest, el
directorio o el nombre del archivo. Esta redundancia deliberada permite que
CoreRPG valide explícitamente el ownership.

El envelope no contiene payload, property bags ni datos arbitrarios.

## `DefinitionReference`

Cada elemento de `references` tiene exactamente esta forma:

```json
{
  "target": "example.test:things/a",
  "expected_kind": "thing"
}
```

`target` es un `DefinitionId` completo y `expected_kind` es el `ResourceKind`
esperado. La definición source se conoce por el documento que contiene el array.

Esta versión no añade `source`, `field_name`, `required`, JSON Pointer, path
físico, punteros persistentes, objetos embebidos ni payload.

## `PackDependency` y `VersionRequirement`

Una dependencia acotada tiene esta forma:

```json
{
  "pack_id": "example.base",
  "version_requirement": {
    "minimum_inclusive": "1.0.0",
    "maximum_exclusive": "2.0.0"
  }
}
```

Una dependencia sin máximo omite `maximum_exclusive`:

```json
{
  "pack_id": "example.base",
  "version_requirement": {
    "minimum_inclusive": "1.0.0"
  }
}
```

La ausencia de `maximum_exclusive` significa que el intervalo no tiene cota
superior. No se utiliza `null`. Los requisitos permanecen estructurados; no se
admiten mini lenguajes textuales como `^1.0`, `~1.0` o `>=1 <2`.

`PackDependency` no contiene ni implica una localización física. En particular,
un `PackId` no se transforma en nombre de directorio, path, URL o entrada de un
registry. La construcción multi-pack sólo considera las raíces que el caller
entrega explícitamente; una dependencia ausente se diagnostica después mediante
`ContentCatalog::create` y nunca provoca discovery o carga automática.

## Frontera de validación

La carga mantiene tres niveles de responsabilidad.

### JSON Schema: forma local

JSON Schema valida el tipo de la raíz, los tipos JSON, campos obligatorios,
campos desconocidos, estructura de objetos y arrays, patrones léxicos simples y
la estructura física de manifests, dependencias, definiciones y referencias.

### Conversión C++: tipos fuertes

La conversión utiliza las fábricas e invariantes existentes para construir
`PackId`, `DefinitionId`, `ResourceKind`, `SemanticVersion` y
`VersionRequirement`. Esto incluye restricciones que no conviene expresar con
patrones complejos, como el rango de 32 bits de cada componente de versión o la
relación entre los extremos de un requisito.

Las operaciones públicas de decoding validan internamente el documento con el
schema de su rol antes de acceder a sus campos. Un fallo de schema conserva
`JsonSchemaDiagnostic`; un fallo posterior de conversión produce un diagnóstico
distinto con código estable, JSON Pointer aproximado y mensaje técnico. La
validación de schema no sustituye nunca a los parsers y factories del Core.

### Loader físico: filesystem y orquestación

El loader lee archivos en modo binario y de forma acotada. Por defecto admite
hasta 16 MiB para `manifest.json` y 64 MiB para cada documento de recurso; el
caller puede sustituir ambos límites mediante opciones explícitas. Un exceso se
conserva como `JsonDiagnosticCode::document_too_large` sin reservar memoria sin
límite.

Los diagnósticos propios del loader cubren únicamente existencia, tipo,
resolución, confinamiento y lectura de archivos. Los fallos del lector estricto,
schema y mapping conservan sus tipos originales junto con el path del documento
y, para recursos, su `ResourceLocator`. No se colapsan en un error genérico.

La operación es atómica: si cualquier recurso falla, no se devuelve un
`ContentPackInput` parcial. Cuando todos funcionan, se construye el input con el
`ContentManifest` decodificado y todas las `ContentDefinitionInput`; los
localizadores no pasan al modelo lógico.

### CoreRPG: significado global

CoreRPG continúa validando:

- `PackId` duplicados;
- ownership de definiciones;
- compatibilidad de `content_contract_version`;
- `minimum_core_version`;
- presencia y versión de dependencias;
- dependencias duplicadas o hacia el propio pack;
- ciclos de dependencias;
- `DefinitionId` duplicados;
- existencia de referencias;
- coincidencia de `expected_kind`;
- autorización cross-pack mediante dependencia directa;
- atomicidad todo-o-nada.

Los schemas no duplicarán la semántica global ya implementada por CoreRPG.

## Campos desconocidos y claves duplicadas

Un campo desconocido en cualquier objeto normativo hace que el documento sea
inválido. Los schemas utilizarán `additionalProperties: false` en los objetos
simples de esta versión. Errores como `pack_verison` no se aceptan ni se ignoran.

Cualquier objeto JSON con dos propiedades del mismo nombre también es inválido.
No se aplica first-wins ni last-wins. La futura capa de lectura debe detectarlo
durante el parsing, antes de que un DOM pueda descartar una aparición. JSON
Schema no es responsable de recuperar información que el parser haya perdido.

## Orden y duplicados en arrays

No tiene semántica el orden físico de propiedades, `dependencies`, `resources`
o `references`. El runtime no depende de ese orden. Para producir documentos y
diffs estables, las herramientas deberían ordenar:

- `dependencies` por `pack_id`;
- `resources` por `ResourceLocator`;
- `references` por `target` y después por `expected_kind`.

Las reglas de duplicidad son:

- un `ResourceLocator` repetido exactamente es un error físico;
- una `DefinitionReference` repetida exactamente es un error físico;
- dos dependencias hacia el mismo `PackId` son un error semántico de CoreRPG,
  aunque expresen requisitos diferentes.

Los requisitos de dependencias duplicadas no se fusionan.

## Schemas, IDs y versionado

Los schemas previstos se organizarán así:

```text
schemas/
  content/
    0.1.0/
      types.schema.json
      content-pack-manifest.schema.json
      resource-definition.schema.json
```

`types.schema.json` contendrá definiciones reutilizables para no duplicar
gramáticas y estructuras. Cada schema declarará JSON Schema Draft 2020-12.

Content Contract `0.1.0` corresponde exclusivamente a los schemas bajo
`schemas/content/0.1.0/`. Una versión futura utilizará otro directorio, por
ejemplo `0.2.0/` o `1.0.0/`. Las versiones históricas serán inmutables y no
existirá un alias `latest` normativo.

Los `$id` utilizarán este namespace:

```text
https://robdor80.github.io/CoreRPG/schemas/content/0.1.0/
```

El namespace no exige publicación web inmediata. `CoreRPGContentJson` embebe
durante el build los tres archivos canónicos versionados de este directorio; no
mantiene copias manuales de su contenido en C++. Un registro local y cerrado
compila los dos schemas raíz como Draft 2020-12 y resuelve sus `$ref` únicamente
contra esos schemas embebidos. Una URI no registrada produce un fallo de
configuración del schema: no existe fallback de red ni búsqueda arbitraria en el
filesystem.

## Vertical neutral de ejemplo

La vertical de prueba utiliza exclusivamente contenido neutral:

```text
content-packs/
  example.test/
    manifest.json
    resources/
      things/
        a.json
        b.json
  example.addon/
    manifest.json
    resources/
      things/
        c.json
```

Contendrá dos definiciones de kind `thing`:

- `example.test:things/a`, sin referencias;
- `example.test:things/b`, con una referencia a `example.test:things/a` y
  `expected_kind` igual a `thing`.
- `example.addon:things/c`, con dependencia directa de `example.test` y una
  referencia autorizada a `example.test:things/a`.

Estos documentos ejercitan la carga física de múltiples `ContentPackInput`, un
requisito de versión y una referencia cross-pack. La orquestación entrega ambos
inputs a `ContentCatalog::create`; el Core sigue siendo la única autoridad sobre
la validación semántica global.

## Frontera con payloads y lore

El envelope genérico compuesto por `id`, `kind` y `references` sólo conserva la
identidad y las relaciones que puede validar el catálogo actual. No pretende
representar todavía los datos de una región, personaje, especie, cultura u otro
concepto concreto.

Antes de crear contenido real de cualquier universo será necesario introducir
al menos un `ResourceKind` concreto y diseñar su payload tipado y versionado.
Ese trabajo deberá mantener una frontera explícita con el envelope genérico.

Content Contract `0.1.0` no introduce `data: {}`, property bags, payloads
arbitrarios ni un equivalente de `std::any`.
