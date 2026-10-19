# ADR-0007 — Contexto estructural autorizado y frontera de IA

- Fecha: 2026-10-19.
- Estado: arquitectura aprobada por el propietario; implementación 4.12 en rama,
  pendiente de auditoría humana y autorización expresa de fusión.
- Base verificada: `4bda9d3cede3c51169dd0b072b1e28be38e66c19`, PR #9 fusionada.
  CI #86 / run `39233038285`: cinco trabajos satisfactorios.
- Continúa ADR-0004, ADR-0005 y ADR-0006; no reemplaza sus autoridades.

## Evidencia y responsabilidades

| Necesidad | Responsable |
| --- | --- |
| Realidad, recuerdos estructurales, proyección autorizada | Core: WorldState, Knowledge, Visibility |
| Seleccionar/copiar hechos de una vista con presupuesto y orden estable | Core: ContextBuilder |
| Memoria episódica reusable, rumores, emociones, calendario, profesiones | Futuro sistema reusable; sin infraestructura especulativa en 4.12 |
| Autenticación, autorización, sesión/campaña, vigencia, secretos y clasificación | Host de confianza y políticas del juego |
| Personalidad, cultura, idioma, escena, sensores, computadora, mando y narrativa | Host/juego; sus documentos no prueban implementación en CoreRPG |
| Serializar contexto, tokens, credenciales, HTTP, modelos, cuotas, reintentos | Adaptadores del Host/proveedor; fuera del núcleo |
| Interpretar/redactar/proponer | Proveedor sin autoridad de ejecución |

Los consumidores de referencia siguen sin acreditar puente nativo. No se cambian
sus repositorios. Su terminología «Core universal» de diseño no incorpora por sí
sola capacidades al C++ de CoreRPG. Auditoría con SHAs y rutas en el acta 4.12.

## Contrato elegido

C++20 tipado, sin nuevo serializador. ContextBuilder recibe exclusivamente una
ObserverView const, solicitud de versión/filtros exactos por sujeto y tipo,
almacenamiento VisibleFact del caller y límites explícitos. No recibe WorldState,
Inspector, snapshot, catálogo, resultados de acciones, RNG ni callbacks.
Con sujeto, lower_bound sobre el orden canónico de facts() localiza su primer
hecho y sólo se recorre su tramo contiguo; sin sujeto se conserva el prefijo de la
vista. El filtro adicional por FactKind sigue combinado con AND; se emiten copias
en orden FactKey. No hay ranking semántico, búsqueda de texto ni inferencia.
Un filtro por ID no conocido devuelve el mismo vacío para oculto/inválido/ausente.
No se generan registros «desconocidos», IDs adicionales ni metadatos globales.

examined cuenta los registros visitados para selección antes del filtro por tipo:
sólo hechos del sujeto solicitado, o del prefijo de la vista si no hay sujeto.
Incluye el registro coincidente bloqueado por selección/capacidad/contenido; excluye
comparaciones de búsqueda binaria y hechos de otros sujetos. Un sujeto sin hechos
autorizados devuelve complete con contadores cero incluso con presupuestos cero,
sin alterar el buffer, igual para oculto/inválido/inexistente.

Salida versionada: estado, examinados, emitidos y coste de contenido; sólo el
prefijo emitido del buffer pertenece a esta llamada. Las copias retienen su texto
inmutable independientemente de la vista/mundo; ninguna revocación puede retirar
copias ya entregadas. El Host conserva el buffer y transmite sólo ese prefijo,
incluido el estado; nunca interpreta truncamiento como desconocimiento.
Completo significa completo respecto al filtro sobre esa vista, no conocimiento
completo del mundo ni existencia de los extremos en el mismo contexto reducido.

Se limita examen, selección, capacidad de salida y contenido. Cero es válido;
excesos de máximos seguros se rechazan antes de escribir. La representación
contabilizada usa anchuras lógicas fijas más bytes de definition_id, sin padding,
sin contar tokens ni prometer tamaño del JSON del Host. El buffer del caller
limita almacenamiento de registros; el algoritmo usa scratch constante y cero
asignaciones. No se añade un parámetro ficticio de scratch dinámico que no usa.
No hay infraestructura de streaming ni segundo almacén de conocimiento.

## Procedencia y compatibilidad

VisibleFact añade un enum y accessor de procedencia: remembered o temporary_grant.
Visibility marca copias de Knowledge como recuerdos y reemplazos concedidos como
actuales autorizados **al proyectar**; no son actualidad perpetua. Una concesión
igual al recuerdo sigue distinguiéndose. present=false conserva ausencia conocida;
find()==nullptr conserva dato no disponible. No se inventan timestamps, fuente
histórica, confianza, precisión o duración. No se refrescan recuerdos no concedidos.

KnownFact, KnowledgeRecord, grants, WorldState y codecs v1/v2/v3 no cambian.
Las concesiones nunca se persisten. La ampliación de VisibleFact es aditiva en
fuente; su igualdad incorpora procedencia deliberadamente. Cambia layout/ABI:
todos los consumidores C++ y puentes deben recompilarse con la biblioteca estática;
no existe ABI binaria estable publicada. No se promete igualdad binaria entre
versiones, STL, arquitecturas o toolchains. No se cambia ninguna firma previa.

## Amenazas y aceptación

- Entradas adversariales: versiones/tipos/límites inválidos se rechazan sin mundo,
  sin escritura; presupuestos agotados se indican inequívocamente, sin desbordar.
- Ocultación: mundos distintos con la misma vista producen contexto y metadatos
  iguales. Consultas repetidas sólo recorren la vista; no hay oráculo del mundo.
- No contaminación: copias y buffers por observador; el Host no serializa colas
  residuales de buffers reutilizados ni mezcla dominios de autorización.
- No mutación/RNG: selección sin APIs privilegiadas; snapshots y RNG idénticos
  antes/después. Pruebas de capacidad y fallo de asignación de la proyección;
  la selección debe funcionar con asignación fallando porque no asigna.
- Persistencia: recuerdos antiguos/ausencias y fixtures históricos siguen válidos.
- Autoridad: propuestas son entrada no confiable, no solicitudes autorizadas.
  Host valida esquema/límites, identidad autenticada, permisos del juego, sesión
  y vigencia; luego referencias/condiciones actuales y ActionEngine sólo para
  contención, relaciones y tiradas soportadas. Actor/revisión no son permisos.
  Igual revisión tras restore no prueba misma campaña ni sesión.
- Textos externos son datos no confiables separados de instrucciones operativas.
  Sanitización o prompts no eliminan inyección. Secretos/clasificación exigen
  filtrado específico antes de cualquier envío externo, también de ampliaciones
  narrativas del Host; nunca usar Inspector como ampliación del prompt.

La frontera acredita no ampliación de información de ObserverView por el builder;
no aislamiento de C++ hostil en el proceso, canales de tiempo/hardware, seguridad
de un Host privilegiado defectuoso o comportamiento universal de un LLM externo.
Cinco trabajos CI completos son puerta de auditoría, no aprobación de fusión.

## Coste y alternativas

Visibility ya copia todos los K recuerdos y procesa G grants; los inserts en
vector pueden mover O(G*(K+G)) registros y reservan O(K+G), además del texto de
definiciones actuales concedidas. ContextBuilder no acota ese coste previo.
Sea N el tamaño de facts() y K_s los hechos del sujeto: selección dirigida
O(log N + K_s), acotada a O(log N + min(K_s, max_examined)). Sin sujeto,
O(min(N, max_examined)) como antes. O(1) scratch; cada copia de texto
es ownership compartido sin copiar caracteres. El Host limita proyección/grants
en su frontera privilegiada. Se medirán separadamente proyección y selección;
sólo evidencia de necesidad justificará una API incremental posterior.

No se añade cliente remoto, ejecutor, propuesta universal, motor narrativo,
memoria, tokens universales, versión de snapshot o integración UE5/Godot/Android.

## Evidencia de implementación para auditoría

El contrato versión 1 se materializa en `context_builder.hpp` y su algoritmo sólo
incluye ese header, algorithm y type_traits: ninguna consulta privilegiada ni callback.
Salida en almacenamiento del caller, sin asignaciones, con máximos 65.536/256/1 MiB.
La falta de scratch dinámico permite límite fijo cero; no se inventa infraestructura
ni parámetro para memoria auxiliar inexistente. No hubo desviación de D1–D4.

Primera auditoría (HEAD eb8ceca), pruebas locales Linux x86_64: 428/428 con
ContentJson, 319/319 Core-only Release y 319/319 ASan/UBSan; biblioteca sin tests/JSON compilada sin terceros. Cuatro puntos
de asignación de proyección inyectados en esta STL; selección funciona con new
fallando. Se comparan mundos ocultos distintos/vistas iguales, restauraciones,
observadores, permisos/grants, extremos, capacidades y máximos. Medidas separadas
confirman que la proyección sigue pagando O(K) antes del presupuesto de selección.

Ver [acta 4.12](../phases/4.12-context-builder-ai.md) para métodos/limitaciones y
[frontera de IA](../ai-host-boundary.md) para responsabilidad del Host. El estado
final de CI Windows/Linux se verifica por SHA en la PR Draft; no hay validación
NDK/ARM64 ni hardware Android, puente de motor o proveedor real en esta fase.

Corrección de primera auditoría: búsqueda dirigida sin cambios de firmas, límites,
VisibleFact/ObserverView ni persistencia. Regresiones tras 4.097 y 65.537 hechos
previos; 65.537 hechos de un solo sujeto agotan los presupuestos de examen; filtros,
orden, buffers, IDs no disponibles, snapshots/RNG y selección con new fallando.
Validación local de la corrección: 433/433 con ContentJson, 324/324 Core-only
Release y 324/324 ASan/UBSan sin informes. Medición dirigida de 65.538 registros:
1 examinado/1 emitido/complete. Detalle en el acta 4.12; CI del nuevo SHA en PR #10.
Las garantías de no ampliación de información, noexcept, cero asignaciones,
no mutación/RNG, procedencia y ausencias conocidas permanecen intactas.
