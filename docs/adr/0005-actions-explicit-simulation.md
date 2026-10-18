# ADR-0005 — Acciones tipadas y simulación causal explícita

- Estado: aceptada para Fase 4.10, pendiente de auditoría humana.
- Fecha: 2026-10-18.
- Base: `e7108e5cd0cc866b33296680ad36dcf670f83443`, PR #7 integrada.

## Evidencia y clasificación

WorldState ya valida identidades, contención acíclica, relaciones, reservas,
orígenes y recuerdos. RuleCheck comprueba hechos sin mutar; RollAction resuelve
una tirada con RNG explícito. RandomStreams conserva secuencias independientes.
Persistence v1/v2/v3 guarda realidad, recuerdos y estados RNG conjuntamente.
No existen propiedades de gameplay, costes, calendario, borrado ni permisos
institucionales. Una unidad latente no es todavía un individuo con EntityId.

| Necesidad | Ubicación y decisión |
| --- | --- |
| Validar y ejecutar solicitudes, referencias, revisiones y precondiciones | Core genérico |
| Orden causal y presupuesto explícito de trabajo | Core genérico |
| Estado y consecuencias confirmadas, continuidad RNG | WorldState y RandomStreams existentes |
| Autenticar actor/origen externo y deduplicar transporte | Host de confianza |
| Seleccionar representaciones, animaciones y FPS | Host |
| Tareas, compromisos y delegación compartidos | Candidato reutilizable; sin invariantes ejecutables comunes todavía |
| Rango, obediencia, autoridad contextual, deliberación e intenciones | Políticas de consumidor; aplazadas |

Tanto individuos de una dotación sin Actor 3D como habitantes sin sprite usan
los mismos EntityId y operaciones. No hay indicador de representación en Core.

## Contrato mínimo

ActionRequest identifica actor/origen lógico existente, una operación tipada,
precondiciones RuleCheck ordenadas y revisión global esperada opcional. El actor
no concede permisos sobre los objetivos: el Host autentica y autoriza la entrada
externa; la API es privilegiada. Un origen NPC utiliza exactamente el mismo
camino. No se interpreta texto, código, scripts, callbacks ni etiquetas de juego.

El conjunto cerrado inicial reutiliza place_entity/remove_containment,
add_relation/remove_relation y resolve_roll. Una solicitud contiene una sola
operación, no una lista arbitraria de efectos ni una transacción de gameplay.
Las primitivas públicas anteriores siguen siendo APIs privilegiadas para
inicialización y compatibilidad; no constituyen otro ActionEngine.

ActionEngine::process es el único ejecutor nuevo. Valida presupuesto de
precondiciones, actor, revisión esperada, conjunción ordenada y operación.
Reutiliza las validaciones de las primitivas, sin copiar WorldState. El resultado
distingue accepted, rejected y no_change, con diagnóstico estructurado, revisión
antes/después y una consecuencia tipada únicamente cuando se publica.
Los rechazos/no-ops no tienen efectos. La aceptación de una tirada devuelve
su valor incluso para un rango singleton; no supone mutación del mundo.

## Invariantes

- Rechazo o excepción: mundo y pool RNG completos intactos. Resultados escalares
  se construyen sin asignar memoria después de la publicación.
- Mutación estructural efectiva aumenta revisión una vez; no-op/rechazo no la
  cambia. Tiradas no cambian revisión del mundo. No hay revisión adicional por
  procesar una solicitud o terminar un paso.
- Revisión esperada se compara antes de evaluar precondiciones. Es una defensa
  conservadora contra entradas obsoletas, no autenticación ni ID global de mundo.
  Una carga puede restaurar revisiones antiguas; el Host delimita sesiones.
- Condiciones se evalúan contra el estado vigente inmediatamente antes de cada
  acción. Un resultado anterior no autoriza una acción posterior.
- Sólo una tirada válida no singleton abre/consume su stream explícito.
  Después de insertar el stream no queda ninguna operación que pueda lanzar.
  Singleton no abre stream; entradas inválidas no lo abren ni consumen.
- No se modifican algoritmo, derivación, distribuciones o vectores RNG históricos.
- No se actualiza Knowledge ni se concede Visibility automáticamente. Diagnósticos,
  resultados y revisiones son privilegiados: no se envían crudos a una UI/NPC.
- Un hilo; el Host excluye mutaciones concurrentes. No hay observación atómica
  entre hilos ni rollback de acciones confirmadas en llamadas anteriores.

## Simulación y recursos

Simulation::step recibe referencias al único mundo/pool, un span de solicitudes
y almacenamiento de resultados del caller. Procesa exactamente en orden del
span, incluidas solicitudes rechazadas; no reordena por actor, prioridad o hash.
El límite de acciones y la capacidad de salida se verifican antes de ejecutar.
Cada acción limita sus precondiciones. No hay reservas implícitas de trabajo,
resultados acumulados, reloj real, ticks persistentes ni cola interna pendiente.

El paso es una secuencia de transacciones por acción, no una transacción del lote.
Ante bad_alloc/length_error devuelve estado de interrupción y cantidad completada:
los resultados de ese prefijo son definitivos, la acción fallida y el sufijo
permanecen intactos. Continuar requiere sólo el sufijo no procesado. No hay
reintento automático. El caller conserva entradas y salida durante la llamada.
Reenviar deliberadamente una tirada es otra tirada: deduplicación de red queda
en el Host. No se promete un límite de tiempo, RAM del mundo o draws de rejection
sampling a partir del presupuesto de acciones.

## Persistencia y alternativas

Las únicas consecuencias duraderas viven ya en WorldState/RandomStreams. No
introducir v4: no existe un tiempo/cola de simulación con estado que guardar.
Guardar/cargar entre llamadas conserva mundo, conocimientos y continuación RNG
sin reproducir acciones. El Host decide qué nueva entrada enviar después de
cargar; entradas pendientes y logs externos no forman parte de CampaignSnapshot.
No se guardan resultados reconstruibles de mutaciones ni proyecciones temporales.

Se rechazan un segundo WorldState, snapshots por cada acción, un dispatcher por
texto, un bus de eventos universal, un scheduler/calendario y un motor NPC.
El RFC de iniciativa se reevaluará tras probar la vertical. Extensiones futuras
requieren evidencia, validación por esta misma autoridad y versión explícita si
añaden estado persistente. No se inicia Inspector 4.11 ni Context Builder 4.12.
