# RFC-0001 — Iniciativa NPC, autoridad contextual, órdenes, solicitudes y delegación

**Estado:** NECESIDAD MULTIUNIVERSO JUSTIFICADA; **ARQUITECTURA E IMPLEMENTACIÓN NO APROBADAS**.  
**Fecha de evaluación:** 2026-10-11.  
**Destino:** evaluación incremental cuando existan Rules, Persistence, Knowledge, Actions y Simulation.  
**Relacionados:** [Normativa multiuniverso](../multi-universe-architecture.md), [Roadmap](../roadmap.md), [ADR-0001](../adr/0001-multi-universe-architecture.md).

> Este RFC registra el resultado de una auditoría de necesidades y una propuesta de clasificación. **No** autoriza nuevas clases, componentes, APIs, esquemas, subsistemas de IA, commits de implementación ni ampliación de 4.5. El diseño mínimo se volverá a decidir a partir de consumidores ejecutables.

## 1. Necesidad y evidencia

Una entidad del universo debe poder **iniciar intenciones y acciones sin intervención del jugador**, encargar trabajo, negociar solicitudes, ejercer autoridad contextual y delegar dentro de límites, sin depender de un Actor de UE5, sprite o llamada obligatoria a una IA remota.

**Star Trek** dispone de contratos de diseño (no implementación de CoreRPG) sobre jerarquía y mando:

- [Cadena de mando](https://github.com/robdor80/Videojuego_StarTrek/blob/main/gameplay/chain_of_command/README.md): la autoridad no se deduce sólo del rango; existen ámbitos, responsables y modos manual, propuesta con aprobación y autonomía delegada.
- [Delegación](https://github.com/robdor80/Videojuego_StarTrek/blob/main/gameplay/chain_of_command/command_decision_delegation_state_v0.1.json): límites, duración, revocación; la delegación no transmite más autoridad que la disponible ni borra la responsabilidad del mando.
- [Órdenes](https://github.com/robdor80/Videojuego_StarTrek/blob/main/gameplay/orders/starship_order_contract.json) y [tareas de subordinados](https://github.com/robdor80/Videojuego_StarTrek/blob/main/gameplay/orders/subordinate_task_order.json): validación, seguimiento, aclaración, negativa y ejecución diferenciadas.
- [Historial operativo](https://github.com/robdor80/Videojuego_StarTrek/blob/main/gameplay/ship_operations/operational_event_log/README.md): registrar hechos no equivale a evaluarlos ni hacerlos conocimiento público.

**Nimroel** contiene normas de diseño para [decisiones y deliberación](https://github.com/robdor80/Videojuego_Nimroel/blob/main/Worldbuilding/Sistemas/Nimroel%20Core/01_Cognicion_y_conducta/decision_deliberacion_y_riesgo_v0.1.md), [objetivos](https://github.com/robdor80/Videojuego_Nimroel/blob/main/Worldbuilding/Sistemas/Nimroel%20Core/01_Cognicion_y_conducta/objetivos_e_intenciones_v0.1.md), [peticiones y consentimiento](https://github.com/robdor80/Videojuego_Nimroel/blob/main/Worldbuilding/Sistemas/Nimroel%20Core/03_Actividad_y_tiempo/peticiones_consentimiento_limites_v0.1.md) y [agenda/compromisos](https://github.com/robdor80/Videojuego_Nimroel/blob/main/Worldbuilding/Sistemas/Nimroel%20Core/03_Actividad_y_tiempo/agenda_personal_uso_tiempo_v0.1.md).

**Nota de nomenclatura:** «Nimroel Core / CORE UNIVERSAL» en el repositorio del juego denomina contratos de diseño del universo. No prueba que ya existan clases ejecutables equivalentes en el repositorio C++ CoreRPG ni determina dónde deben implementarse.

Los dos consumidores demuestran el valor de acciones autónomas y compromisos con historia, pero **no** prueban que compartan una jerarquía universal, una política de obediencia o un único algoritmo de deliberación.

## 2. Estado técnico real al evaluar

En la rama pública de 4.5, CoreRPG tiene `EntityId`, `WorldState` con identidad/contención/revisión y `DeterministicRandom`/`RollAction` mínimos; ContentCatalog maneja definiciones declarativas. La fase 4.5 sigue abierta, con Incremento 3 pendiente de revisión y publicación desde el entorno local.

**No hay** un ActionEngine general, un modelo persistente de tareas, autoridad contextual, delegaciones, agendas, eventos autoritativos, simulación NPC, Knowledge completo ni proveedores de IA integrados. No duplicar ni presentar como código los contratos existentes de los juegos.

## 3. Distinciones e invariantes para el análisis futuro

Deben evaluarse de modo independiente:

1. **Legitimidad de emitir** una instrucción o actuar en nombre de alguien: competencia, ámbito, vigencia y límites según reglas del juego.
2. **Carácter obligatorio o voluntario** de lo solicitado: una petición entre iguales no se transforma en orden por tener un emisor; una orden legítima no garantiza obediencia.
3. **Posibilidad material de ejecutar:** entidad, recursos, capacidades, estado, disponibilidad, tiempo, seguridad y restricciones.
4. **Elección del destinatario:** aceptar, aclarar, negociar, rechazar, desobedecer, delegar donde esté permitido, aplazar o abandonar.
5. **Aplicación real y consecuencia:** elegir o aceptar no significa ejecutar, consumir recursos ni completar un objetivo.

**Dos autoridades distintas:** (a) la autoridad técnica del Core sobre mutaciones del WorldState, innegociable; (b) autoridad social, institucional, laboral o jurídica **dentro de la ficción**, definida por cada universo. Una orden narrativa puede ser ilegítima y aun así un personaje puede intentar ejecutarla; Core deberá validar la acción física y registrar hechos y posibles infracciones según las reglas aplicables.

Diferenciar asimismo **orden**, **petición**, **tarea/compromiso**, **política sostenida** y **delegación de autoridad**; pueden relacionarse, pero no comparten necesariamente un único ciclo de estados.

Una delegación de tarea no otorga competencias generales. Delegar competencia no transfiere automáticamente la responsabilidad última, según la política del universo; su validez puede vencer o revocarse. Las delegaciones no pueden conceder autoridad superior a la disponible.

Las decisiones de NPC y propuestas de IA requieren conocimiento **limitado por observador**. El hecho registrado no equivale a un recuerdo o conocimiento automático de terceros. La intención no altera por sí sola el estado autoritativo.

## 4. Clasificación propuesta (no aprobación de ubicación definitiva)

| Necesidad | Ubicación candidata | Motivo |
| --- | --- | --- |
| Validación y ejecución de una acción estructurada | CoreRPG / Actions 4.10 | Autoridad técnica única, útil para jugador, NPC, Host y Companion |
| Reglas/condiciones reutilizables, resultado explícito | CoreRPG / Rules 4.6 y posteriores | Sin codificar jerarquías específicas |
| IDs y conservación de hechos/estado | CoreRPG, en fases apropiadas | Identidad, invariantes, trazabilidad |
| Proyecciones de conocimiento | CoreRPG / Knowledge 4.9 | No filtrar información secreta |
| Seguimiento de tareas, compromisos y delegaciones | **Sistema reutilizable candidato**, ubicación por decidir | Comprobar invariantes reales de ambos juegos antes de introducir API |
| Evaluación de autoridad contextual | **Contrato mínimo candidato**, políticas por universo | Evitar rango numérico o jerarquía universal |
| Organización militar, cadena de mando, obediencia, leyes, mando de nave | Star Trek/Nimroel según corresponda | Semántica particular del universo |
| Deliberación, objetivos, estrategia de NPC, prioridades y agenda | Sistemas de comportamiento/simulación + políticas específicas | No imponer un único cerebro universal |
| IA remota generativa | Host/adaptador opcional | La respuesta no modifica directamente WorldState |

La **iniciativa** es la facultad de originar una solicitud al mismo camino de validación, **no** una autorización especial para saltarse Rules/Actions ni un segundo ActionEngine.

## 5. Riesgos y límites

- **Sobreingeniería:** no crear `UniversalCommandSystem`, ECS, framework de agentes ni jerarquía general antes de probar el contrato mínimo.
- **Conflictos operativos:** tareas simultáneas incompatibles, reservas de recursos, versiones obsoletas, revocaciones y autorización contextual variable requieren reglas de serialización/arbitraje que se decidirán cuando existan Actions/Time/Simulation.
- **Persistencia:** el estado e historial necesario de encargos o delegaciones no debe desaparecer al descargar escenas o guardar la partida; el contrato exacto se decide después de 4.7.
- **Determinismo:** propuestas externas no son necesariamente reproducibles; registrar las decisiones aceptadas y entradas suficientes. Revalidar tras la latencia de una API o un cambio de estado.
- **LOD y coste:** no deliberar por frame ni consultar IA para cada NPC; usar reglas locales, eventos, agendas y frecuencias según relevancia. No prometer cuotas sin benchmarks.
- **Seguridad y secretos:** el conocimiento autorizado y autoridad narrativa son distintos de permisos técnicos de integraciones externas.
- **Capacidades compartidas:** un tercer universo sin rangos ni instituciones debe poder usar acciones y peticiones sin heredar cadena de mando militar.

## 6. Hitos de reevaluación, no implementación adelantada

- **4.5:** cerrar alcance actual sin ampliar API.
- **4.6:** diseñar Rules de modo que admita evaluación de condiciones de origen/contexto sin introducir jerarquía ficticia.
- **4.7:** permitir persistencia/restauración futura de hechos y referencias duraderas; no añadir campos especulativos.
- **4.8 (por revisar):** contrastar entidad, actor, actividad, población y relaciones sin asumir modelos humanoides universales.
- **4.9:** conocimiento/visibilidad según observador; sin acceso del NPC al estado oculto.
- **4.10:** primero Actions + Simulation mínimo. **Después**, RFC específico y contrato mínimo de solicitudes/compromisos/autoridad/delegación únicamente si los consumidores ejecutables lo justifican. No duplicar el ejecutor de acciones.
- **4.12:** integración opcional de propuestas de IA, sin autoridad ni dependencia de disponibilidad remota.

No se reserva nueva numeración de fase. La ubicación del sistema de compromisos/delegaciones sigue **sin aprobar**.

## 7. Criterios mínimos de aceptación para una eventual implementación

1. Un capitán NPC puede tomar decisiones operativas sin intervención del jugador ni Actor 3D cargado; las acciones siguen el sistema autoritativo.
2. Un alférez puede recibir instrucciones sin adquirir por ello autoridad de capitán.
3. Un capitán jugador puede decidir legítimamente aceptar/modificar/rechazar/delegar dentro de límites; un valor recomendado no impone automáticamente la decisión.
4. Una solicitud entre iguales puede ser aceptada o rechazada sin fabricar superioridad jerárquica.
5. La delegación expirada/revocada no habilita **nuevas** decisiones; conserva hechos e historial anteriores.
6. Una orden emitida/aceptada no se confunde con ejecución completada ni recursos creados.
7. Tareas incompatibles no se ejecutan simultáneamente contra disponibilidad o recursos inexistentes.
8. La simulación fuera de escena y la carga de partidas mantienen identidad, decisiones y consecuencias; no recomienzan tareas arbitrariamente.
9. La propuesta de IA se revalida antes de ejecutar y su caída no bloquea emergencias ni actividades esenciales.
10. Los registros no revelan automáticamente hechos ocultos a NPC o jugador.

## 8. Decisión de esta revisión

**Aceptar el problema compartido; aplazar la forma técnica.** No implementar en 4.5, no alterar sus contratos y no crear subsistemas paralelos. Retomar la evaluación junto a Actions/Simulation, preservando los requisitos verificados de Star Trek, Nimroel y casos sin jerarquía.

Este RFC será sustituido o complementado por una ADR de diseño **sólo si** se decide una interfaz definitiva y se autorizan cambios.
