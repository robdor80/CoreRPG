# Formato CoreRPG v3 — Knowledge 4.9

V3 es explícito y conserva [v1](persistence-format-v1.md) y
[v2](persistence-format-v2.md) sin modificar sus bytes. Política RNG 1 y CRC32C
sin cambios. Capture selecciona v3 sólo si hay Knowledge; en otro caso conserva
la selección mínima histórica. Deserialize/serialize conservan la versión
recibida. Un v1/v2 con Knowledge se rechaza, nunca se descarta silenciosamente.
Cargar formatos antiguos reemplaza Knowledge por vacío, sin inferencias.

## Layout normativo

Little-endian, octetos de 8 bits, sin padding. Header v2 con versión 3; después
del count M en offset 80 se añade count K (uint64) en offset 88. Los records
empiezan en 96. Siguen las mismas seis secciones v2, luego K records Knowledge
y CRC32C uint32. V3 vacío ocupa 100 octetos.

Cada record Knowledge tiene 62 octetos fijos y texto opcional:

| Offset relativo | Campo | Ancho |
| --- | --- | --- |
| 0 | observer EntityId | uint64 |
| 8 | subject EntityId | uint64 |
| 16 | FactKind: existencia=1, definición=2, contención=3, relación=4, reserva=5, origen=6 | uint8 |
| 17 | relation_kind (sólo relación) | uint64 |
| 25 | target (sólo relación) | uint64 |
| 33 | present: exactamente 0 o 1 | uint8 |
| 34 | reference (contenedor u origen) | uint64 |
| 42 | total (reserva) | uint64 |
| 50 | latent (reserva) | uint64 |
| 58 | longitud de DefinitionId | uint32 |
| 62 | texto ASCII de DefinitionId, sin terminador | longitud octetos |

Tamaño exacto: tamaño v2 de las seis secciones + 8 + 62K + suma de longitudes
de definiciones recordadas. Los campos no utilizados son cero o texto vacío;
se rechazan campos sobrantes. Sólo una definición presente lleva texto válido.
Existencia siempre presente; ausencia conocida de contenedor/origen usa referencia
cero, ausencia de reserva total/latent cero. Una relación conserva presencia o
ausencia del triple exacto, sin convertirlo en listado exhaustivo de relaciones.

## Validación y autoridad

Orden estricto por (observer, subject, FactKind, relation_kind, target). Se
rechazan duplicados, versiones/tipos desconocidos, flags no canónicos, referencias
no existentes y extremos sin existencia conocida para ese mismo observador.
Contención recordada no permite self-link; origen recordado es anterior al sujeto;
reservas cumplen latent <= total. No se compara un recuerdo con el estado actual:
puede representar un vínculo retirado, una ausencia anterior o una latencia vieja.
No se interpreta como realidad alternativa ni se aplica al mundo al restaurar.

La revisión mínima añade K al mínimo v2, comparando por restas antes de sumar.
Cada record vigente necesitó al menos una operación efectiva; no se reconstruye
el historial completo. Revocar varios records es una sola mutación.

PersistenceLimits añade max_knowledge (1 000 000); max_bytes, max_definition_bytes
y sus límites históricos siguen aplicándose. Counts/tamaños mínimos, CRC,
longitudes y sufijo exacto se verifican antes de reservar records. Los cálculos
comprueban límites antes de multiplicar/sumar y no recorren el intervalo de IDs.
El fixture independiente de 240 octetos cubre un stream máximo y dos recuerdos,
incluida ausencia conocida de definición.

Restore/load resuelven también definiciones recordadas contra el catálogo
explícito. Catálogo ausente/incompleto rechaza sin publicar. Textos compartidos
son inmutables; las tablas de mundo, copias y snapshots son independientes.
Se preparan ambos destinos y se publican todas las tablas y RNG mediante swaps
noexcept. Excepción o rechazo conserva estado y referencias previas.

Knowledge se valida en O(K log(K+1)), además de los costes v2, con almacenamiento
O(K + texto). Las proyecciones y concesiones temporales no se serializan.
Snapshots y bytes completos son privilegiados: nunca se entregan a clientes que
sólo están autorizados a recibir ObserverView. CRC detecta corrupción, no autentica.
I/O físico, sincronización de acceso y selección de contenido pertenecen al Host.
