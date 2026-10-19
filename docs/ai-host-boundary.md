# Intercambio de contexto y propuestas desde un Host

El contrato C++ versión 1 está en `corerpg/context_builder.hpp`; no es un protocolo
remoto. [Ejemplo compilado](../examples/authorized_context_host.cpp) y
[política ilustrativa](../examples/authorized_context_host.hpp) viven fuera de la
API pública. No hay proveedor, autenticación real, parser, sistema de órdenes ni
motor narrativo en el ejemplo. La política sólo permite contención del propio
actor y debe sustituirse por las autorizaciones del juego en una integración.

## Entrega autorizada

1. Host autentica destinatario y decide permisos y grants; sólo él usa Visibility.
2. ContextBuilder recibe la vista const y filtros exactos; el caller aporta buffer.
3. Consumir **únicamente** `storage.first(result.emitted)` junto a
   `result.contract_version` y `result.status`. La cola puede contener valores
   de una llamada anterior. Estado distinto de complete no certifica cobertura.
4. El Host serializa campos lógicos por nombre/tipo, nunca `memcpy` de objetos
   C++/padding/punteros. Conserva procedencia y ausencia conocida. No aporta
   timestamps/confianza que no existen. Una definición es un identificador,
   no el contenido de un catálogo ni una biografía.
5. El adaptador impone tamaño del envelope/serialización y tokens del proveedor.
   `content_bytes` sólo contabiliza payload lógico de hechos, no JSON ni tokens.
   No enviar contadores/revisiones de WorldState, Inspector, snapshots o
   ActionResult. Los contadores del builder derivan exclusivamente de la vista.
6. Antes de enviar al exterior filtrar secretos de personajes y clasificación
   institucional según juego, destinatario y política de tratamiento de datos.
   También los textos de memoria/escena/persona deben superar la autorización;
   no obtenerlos de tablas privilegiadas para «enriquecer» el prompt.

El resultado no certifica conocimiento global, ni actualidad de recuerdos, ni
cierre referencial del subconjunto seleccionado. Un registro ausente del
contexto puede estar filtrado/truncado; no equivale a ausencia conocida. Un grant
es actual al proyectar y puede caducar inmediatamente después. Para retirar un
grant, regenerar vista/contexto; reemplazar caches y cancelar entregas pendientes.
Las copias y datos ya enviados siguen disponibles al receptor: el Host gestiona
retención/revocación externa sin prometer borrado retroactivo.

## Propuestas no confiables

El proveedor interpreta, redacta o propone. Un texto «autorizado» o la aparición
de un ID en el contexto no da permiso para actuar. Antes de ejecutar:

1. Validar esquema cerrado, operación admitida, campos, tipos, tamaños y límites.
2. Resolver actor mediante identidad autenticada del Host; no confiar en actor
   declarado por el modelo. Rechazar suplantación y transporte no autenticado.
3. Autorizar objetivos/operación con reglas del juego en ese instante. Rango,
   profesión, relación, conocimiento de existencia y grants no son permisos.
4. Verificar correlación de solicitud, sesión/campaña, caducidad y deduplicación.
   Mantener binding privado del contexto emitido. Tras restore/reemplazo invalidar
   la sesión/generación incluso si revisión y EntityIds coinciden. No exportar la
   revisión privilegiada al modelo para que la devuelva como supuesto permiso.
5. Revalidar referencias, condiciones y estado actual bajo exclusión de mutación
   concurrente. Una vista antigua o un resultado anterior no bastan.
6. Para una operación soportada crear ActionRequest y usar ActionEngine::process
   (o Simulation con sus límites). Éstos sólo implementan contención, relaciones
   y tiradas; revisión/precondiciones/primitivas revalidan ese alcance. No validan
   permisos del juego, costes, órdenes de mando, transacciones de gameplay,
   narrativa, promesas o memoria episódica.

El ejemplo ejercita esta división para una sola operación; omite deliberadamente
red/parser/reloj/deduplicación. Sus inputs de sesión y permiso proceden del Host
de confianza, no de un paquete del proveedor. ActionResult queda en el servidor;
para mostrar consecuencias se regenera ObserverView con la política vigente.
No reenviar automáticamente acciones parcialmente completadas ni repetir tiradas.

## Inyección y ampliaciones

Documentos, conversación previa y textos externos son datos no confiables en
campos separados de las instrucciones operativas del adaptador. Instrucciones
contenidas en ellos no cambian permisos ni habilitan herramientas. Límites,
sanitización o prompts no eliminan completamente la inyección. El acceso a
operaciones se controla independientemente de la salida textual del modelo.

No se puede probar que cualquier LLM nunca verbalice datos inesperados. Sí se
puede probar que ContextBuilder no añade información a la vista autorizada.
Las ampliaciones del Host deben respetar esa frontera; su prueba real requiere
los futuros puentes, sistemas narrativos y autorizaciones de dominio.
