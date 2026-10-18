# Formato de guardado CoreRPG v2 — estado estructural 4.8

Contrato implementado en Fase 4.8 e integrado mediante PR #6 (`d6e2c8f`).
No modifica [v1](persistence-format-v1.md). La política RNG sigue siendo 1;
SplitMix64, derivación y mapeo de rangos no cambian.

## Selección y compatibilidad

`save_format_version = 1` conserva su valor histórico;
`save_format_version_v2 = 2` identifica el contrato extendido.
CampaignSnapshot sigue aceptando candidatos v1/v2. Capture selecciona v1 si
todo el estado es representable en v1; selecciona v2 si hay bindings,
relaciones, reservas, orígenes o una revisión no representable por las
invariantes v1. El caller puede marcar explícitamente un candidato sin
extensiones como v2. No se descartan campos al seleccionar v1.
Bytes→snapshot→bytes conserva la versión recibida. Cargar y volver a capturar
el mundo selecciona otra vez el mínimo sin pérdida; la versión física original
no es estado autoritativo. Por ejemplo, un v2 vacío explícito se recaptura como v1.

Serialize rechaza un v1 con cualquiera de los campos nuevos. Deserialize
interpreta v1 sin migrar sus bytes: todos los campos extendidos están vacíos.
Restaurar un v1 reemplaza también las tablas nuevas del destino por esas tablas
vacías; no las conserva accidentalmente. Un consumidor 4.7 rechaza v2 como
versión desconocida. Versiones diferentes de 1 y 2 siguen rechazadas.

## Codificación normativa

Octetos de 8 bits, uint32/uint64 little-endian, sin padding, layout C++, punteros,
locale o datos del Host. Magic y CRC32C son los mismos que en v1. CRC cubre
todos los octetos anteriores al checksum y no constituye autenticación.

| Offset | Campo | Tipo |
| --- | --- | --- |
| 0 | ASCII CRPGSAVE | 8 octetos |
| 8 | versión = 2 | uint32 |
| 12 | política RNG = 1 | uint32 |
| 16 | último ID emitido | uint64 |
| 24 | revisión | uint64 |
| 32 | master seed | uint64 |
| 40 | cantidad L de vínculos de contención | uint64 |
| 48 | cantidad S de streams materializados | uint64 |
| 56 | cantidad D de bindings a definición | uint64 |
| 64 | cantidad R de relaciones | uint64 |
| 72 | cantidad P de reservas | uint64 |
| 80 | cantidad M de orígenes individualizados | uint64 |
| 88 | secciones siguientes en orden fijo | records |

Secciones en orden, sin alineación ni terminadores:

1. L pares (entidad, contenedor), 16 octetos cada uno, orden por entidad.
2. S pares (stream ID, estado), 16 octetos, orden por stream ID.
3. D bindings: entidad uint64, longitud uint32 y exactamente esa cantidad de
   octetos ASCII canónicos de DefinitionId. Orden por entidad.
4. R triples (source, kind, target), tres uint64, orden lexicográfico numérico
   por source, kind, target.
5. P triples (group, total, latent), tres uint64, orden por group.
6. M pares (entidad emitida, reserva de origen), dos uint64, orden por entidad.
7. CRC32C uint32 final; sin sufijos ni bytes desconocidos.

Tamaño exacto:
`92 + 16L + 16S + 12D + suma(longitudes de DefinitionId) + 24R + 24P + 16M`.
Cada operación de suma/multiplicación se comprueba antes de realizarla. Un
guardado v2 vacío ocupa 92 octetos; el mismo estado capturado automáticamente
continúa usando el v1 de 60 octetos.

## Referencias declarativas

Un binding conserva sólo EntityId→DefinitionId, no ResourceKind, manifest,
pack_version ni payload. La gramática ASCII es exactamente la de DefinitionId;
se valida sin construir identificadores ni realizar I/O. No son paths.
Deserialize obtiene un candidato poseído por valor con texto canónico.
Restore/load requieren un ContentCatalog explícito cuando hay bindings y
resuelven cada texto contra sus identidades validadas antes de construir el
mundo. Catálogo ausente o definición ausente implica rechazo atómico.

La selección de publicaciones de contenido corresponde al caller. No se
promete que cambiar contenido externo conservando un ID mantenga su significado;
pinning de packs, payloads ricos y migraciones de instancias son trabajo futuro.
La carga no descubre, descarga ni ejecuta Content Packs.

## Validación del mundo

Además de identidad, contención y RNG de v1:

- Revisión al menos `last ID + L + D + R + P`, comparada por restas sin overflow.
  M no suma revisiones adicionales: los orígenes se registran dentro de las
  creaciones elementales. Con last ID cero, revisión cero. V2 permite una sola
  entidad con revisión mayor debido a relaciones, bindings o reservas.
- Bindings: entidad existente, texto válido, una definición por entidad,
  orden estricto. Restore verifica existencia en catálogo.
- Relaciones: extremos existentes, kind distinto de cero, triple único y
  orden estricto. Ciclos/autorreferencias son válidos en este grafo genérico.
- Reservas: group existente y único, latent ≤ total, orden estricto.
- Orígenes: entidad existente y única, reserva declarada existente,
  origen.group < entidad emitida, orden estricto.
- Para cada reserva, `total - latent` coincide exactamente con el número de
  orígenes individualizados. No hay entidades fantasma, duplicadas ni reservas
  reabastecidas al cargar.

La revisión mínima comprueba invariantes, no reconstruye todo el historial.
El orden de las tablas se rechaza si es no canónico; no se normaliza una entrada
corrupta o ambigua. Cargar no usa mutaciones ordinarias ni aumenta revisión.

Desde 4.9 también existe [v3](persistence-format-v3.md). Los bytes y la semántica
v2 permanecen intactos; v2 carga Knowledge vacío y no admite ese campo extendido.

## Límites, errores y coste

PersistenceLimits mantiene sus tres campos v1 y añade max_definitions,
max_relations, max_populations y max_memberships (1 000 000 cada uno), y
max_definition_bytes (4096 por defecto, también sujeto al ancho uint32).
max_bytes sigue en 64 MiB; son políticas configurables, no límites de gameplay.
No limitan la reserva latente, total ni el intervalo implícito de entidades.

Antes de reservar records se comprueban counts, tamaño mínimo, CRC y un recorrido
sin asignación de longitudes/textos y sufijo exacto. Después se valida el
candidato entero y el catálogo antes de construir ambos valores temporales.
Los constructores privados y movimientos de snapshots propagan asignación
fallida también con los proxies de iteradores MSVC. La publicación es por swaps
noexcept de todas las tablas, contadores y RNG. Rechazo/exception conserva ambos
destinos; éxito invalida referencias previas. No hay concurrencia ni atomicidad
de filesystem; el Host coordina y almacena los bytes.

Coste de validación adicional O(D + R + P + M log(P+1)), más gramática/texto y
contención O(L log(L+1)), con O(P+L) memoria auxiliar. No itera hasta total o
last ID. La representación visual no forma parte del contrato.

El fixture dorado independiente de 207 octetos incluye binding, relación,
reserva, origen y un stream con ID UINT64_MAX. Se verifica en ambas plataformas
junto con v1, round-trip, continuidad y fallos de memoria. Resultados reales
en [Fase 4.8](phases/4.8-entities-population.md). No se acredita Android ni UE5.
