# Formato de guardado CoreRPG v1

Especificación del contrato de Fase 4.7, pendiente de revisión e integración.
`save_format_version = 1` es independiente de la versión de biblioteca y de
los contratos de Content Packs. No implica compatibilidad con versiones
desconocidas ni migraciones automáticas.

## Datos auditados y fronteras

- WorldState: último EntityId emitido, revisión y vínculos directos. Actualmente
  todos los IDs de `1` hasta el último existen y no hay borrado. El intervalo
  codifica exactamente las identidades y el asignador; no se enumera hasta
  UINT64_MAX ni se guarda un contador redundante. Cambiar este modelo exige otra
  versión del formato, sin cambiar el contrato público de enumeración de 4.5.
- RandomStreams: master seed y pares ID/estado de streams materializados.
  Estado significa estado SplitMix64 actual, no semilla inicial ni última salida.
  Los streams ausentes siguen ausentes. La política RNG 1 conserva íntegros el
  algoritmo, derivación y rechazo de rangos de 4.6. Todos los uint64, incluido
  cero y UINT64_MAX, son válidos como semillas, estados e IDs de stream.
- ContentCatalog, manifests, definiciones y schemas: contenido declarativo
  inmutable, no realidad runtime. No existe puente DefinitionId→EntityId que
  persistir. No se incorporan referencias especulativas a packs.
- Host: archivos, paths, UI, actores, sprites, plataforma, red y durabilidad.
  No se guardan en este contrato. El Host transporta bytes; no accede a storage
  mutable del Core. Debe coordinar un punto sin mutaciones concurrentes.

## Codificación normativa

Octetos de 8 bits; enteros unsigned de ancho fijo, little-endian mediante
desplazamientos explícitos. Sin padding, terminadores, punteros, layout nativo,
locale, hora o std::hash. No hay campos opcionales ni bytes sobrantes.

| Offset | Campo | Ancho |
| --- | --- | --- |
| 0 | ASCII `CRPGSAVE` | 8 octetos |
| 8 | versión de formato = 1 | uint32 |
| 12 | política RNG = 1 | uint32 |
| 16 | último EntityId emitido | uint64 |
| 24 | revisión técnica | uint64 |
| 32 | master seed | uint64 |
| 40 | cantidad L de vínculos | uint64 |
| 48 | cantidad S de streams materializados | uint64 |
| 56 | L pares (EntityId, contenedor) | 16 octetos/par |
| 56 + 16L | S pares (stream ID, estado) | 16 octetos/par |
| 56 + 16(L+S) | CRC32C de todos los octetos anteriores | uint32 |

Tamaño exacto: `60 + 16*(L+S)` octetos, con cálculo comprobado antes de
reservar memoria. Vínculos en orden numérico estrictamente ascendente por
entidad; streams en orden numérico estrictamente ascendente por ID. Duplicados
y entradas desordenadas se rechazan, no se normalizan silenciosamente.

CRC32C Castagnoli reflejado: polinomio `0x82F63B78`, estado inicial
`0xFFFFFFFF`, XOR final `0xFFFFFFFF`, actualización bit a bit por octeto.
El vector independiente ASCII `123456789` produce `0xE3069283`. El checksum
detecta corrupción accidental; no proporciona autenticación, cifrado ni prueba
de que un usuario no haya editado el estado. El Host añade seguridad externa si
la necesita. Un cambio válido con CRC recalculado sigue sujeto a invariantes.

## Validación y compatibilidad

Versión y política deben coincidir exactamente. Longitud, límites y counts se
comprueban sin desbordar y antes de asignar records; después se comprueba CRC.
No se aceptan truncamientos, sufijos, referencias inexistentes, ID entidad cero,
autocontención, más de un padre, ciclos ni revisiones inferiores al número de
entidades más vínculos actuales. Un mundo con cero o una entidad exige revisión
igual al conteo: todavía no puede haber tenido cambios de contención.
La validación no pretende reconstruir todo el historial de mutaciones.

Los límites por defecto son 64 MiB, 1 000 000 vínculos y 1 000 000 streams;
el llamador puede escoger otra política explícita. No limitan el intervalo de
identidades implícito. La implementación debe acotar coste por records, no por
último ID ni por profundidad recursiva. Se rechazan tamaños no representables
en la plataforma. Los fallos de asignación propagan excepciones estándar.

Un snapshot es un candidato poseído por valor. Validarlo no le concede acceso
mutable ni autoridad sobre el mundo. Restaurar revalida, prepara ambos valores
y publica mediante swaps sin excepciones, sin create_entity(), place_entity(),
stream() ni tiradas. El destino y RNG originales permanecen intactos ante
error. Éxito reemplaza ambos y anula referencias previas a streams. La
atomicidad es de una llamada sin concurrencia, no una transacción de disco.

No hay migradores, forward compatibility, blobs desconocidos, compresión ni
historial de RNG. Las mismas entradas y operaciones siguientes deben producir
los mismos resultados; otros RNG privados del Host quedan fuera del snapshot.
La compatibilidad Windows/Linux se comprobará con bytes dorados compartidos,
round-trip, continuidad y CI en MSVC/GCC. Android y Unreal requieren verificaciones
de sus integraciones propias y no se acreditan con esta especificación.
