# CoreRPG — Roadmap vivo: fases, plataformas y Hosts

**Fecha de revisión:** 2026-10-18.
**Referencias:** [Arquitectura](architecture.md), [ADR-0001](adr/0001-multi-universe-architecture.md), [ADR-0002](adr/0002-platforms-presentation-companion.md), [4.5 cierre](phases/4.5-generic-world-state.md), [4.6 plan](phases/4.6-rng-rules.md), [RFC iniciativa NPC](proposals/npc-initiative-authority-delegation.md).

Este documento añade **criterios transversales** a la hoja de ruta incremental. No renumera fases ni atribuye al Core capacidades no implementadas. El Core seguirá siendo un único núcleo C++20 independiente de la tecnología de presentación.

## Estado de partida

- Fases **4.1–4.5 cerradas**. La fase 4.5 (Generic World State) se integró en `main` mediante PR #3, merge `d23c400`, con CI de `main` Windows/MSVC y Linux/GCC superada (run `38365231770`). Véase el [acta 4.5](phases/4.5-generic-world-state.md).
- **4.6 — RNG + Rules:** fusionada mediante PR #4, merge `8f1cbf72003631706e96ce79100b7dd504268ed0`; CI de `main` Windows/MSVC y Linux/GCC superada ([run 38374915709](https://github.com/robdor80/CoreRPG/actions/runs/38374915709)). Documento histórico en [4.6](phases/4.6-rng-rules.md).
- **4.7 — Persistence:** integrada mediante PR #5, merge `483ad13ff9d68fc3f23146b4f01e26904d5ac3a9`, CI de main Windows/MSVC y Linux/GCC superada ([38381606241](https://github.com/robdor80/CoreRPG/actions/runs/38381606241)). Evidencias en [4.7](phases/4.7-persistence.md).
- **4.8 — Entidades/población/relaciones:** integrada mediante PR #6, merge `d6e2c8fe5a33405bc41b082cf89d6c8036f3c8b8`. Clasificación y garantías en [ADR-0003](adr/0003-generic-population-relations.md) y [Fase 4.8](phases/4.8-entities-population.md).
- El estado de una rama no acredita que la fase ya esté fusionada en `main`.
- Aún no están implementados: registro rico de personajes, horario/estado vivo completo, simulación fuera de escena, almacenamiento físico del Host y migraciones de guardados, autoridad contextual/órdenes ejecutables, módulos de Host, Companion y sus HUD. Los contratos de los juegos no equivalen a una implementación C++ del Core.

## Plataformas y presentaciones fijadas

| Producto | Plataforma | Representación | Interfaz obligatoria |
| --- | --- | --- | --- |
| Star Trek | Windows/MSI | UE5, 3D | Teclado/ratón; jugable sin tablet |
| Nimroel Windows | Windows/MSI | Godot, estrictamente 2D | HUD completo teclado/ratón |
| Nimroel Android | Android ARM64 / Galaxy Tab S9+ | Godot, estrictamente 2D | HUD completo táctil, sin MSI ni red |
| RobDev Companion | Tablet conectada a juego Windows | UI auxiliar, no videojuego | Opcional, sin autoridad de partida |

**Invariante:** actor UE5 o representación 2D ausente NO significa entidad inexistente. No mantener un segundo WorldState de la campaña en la presentación.

## Fases funcionales: interpretación sin renumeración

| Fase | Alcance y nueva precaución |
| --- | --- |
| **4.5 Generic World State** | **CERRADA**, con identidad, revisión, enumeración, contención y tests integrados. Sin Android, UE5, Companion, serialización o modelos de dominio. |
| **4.6 RNG + Rules** | **FUSIONADA:** ownership de streams RNG y reglas estructurales puras; conserva SplitMix64, sin dependencia gráfica, canon ni ActionEngine 4.10. |
| **4.7 Persistence** | **FUSIONADA:** identidades, asignador, revisión, vínculos, master seed y streams exactos. Codec v1 y restauración atómica. |
| **4.8 Entidades/población/relaciones** | **FUSIONADA (PR #6):** reservas agregadas, emisión acotada, origen inmutable, bindings y grafo genérico. Persistencia v2 con lectura v1 intacta. Familias y rango fuera del Core. |
| **4.9 Knowledge & Visibility** | **IMPLEMENTADA EN RAMA, PENDIENTE DE AUDITORÍA:** recuerdos parciales por observador, proyecciones sin metadatos globales y persistencia v3. Véase [4.9](phases/4.9-knowledge-visibility.md). |
| **4.10 Actions + Simulation** | Solicitudes estructuradas y resultados validados por autoridad técnica única; simulación off-screen causal. **Después del mínimo funcional**, reevaluar la propuesta de iniciativa, autoridad contextual, compromisos y delegación; no crear segundo ActionEngine. |
| **4.11 Inspector** | Observación no mutante del estado lógico; diagnóstico para Hosts y validación. |
| **4.12 Context Builder + AI** | Contextos autorizados; propuestas de IA opcionales sujetas a revalidación; sin mutación directa ni dependencia obligatoria de API remota. |
| **4.13 Primer Content Pack Nimroel** | Contenido real y validación de un consumidor, sin imponer un renderizador. |
| **4.14 Integración prevista con Unreal** | Vinculada específicamente al Host **Star Trek/UE5**. **No** obliga a Nimroel a usar Unreal. La integración de Nimroel 2D se planificará en paralelo como Host independiente, sin cambiar por ahora los números de fase. |
| **4.15 Vertical slice Treskal** | Demostración real Nimroel en 2D; pruebas del estado, la presentación y la continuidad. |

No significa que todos los conceptos de las fases posteriores pertenezcan al Core: aplicar siempre la clasificación Core / reusable / específico de ADR-0001.

## Hitos transversales y orden recomendado

### P0 — Fase 4.5: cierre confirmado (COMPLETADO)

- PR #3 fusionada en `main`, commit `d23c400`; tres incrementos integrados.
- CI de `main`: Windows/MSVC y Linux/GCC correctas, run `38365231770`.
- Acta histórica: [4.5 Generic World State](phases/4.5-generic-world-state.md). No confundir persistencia lógica en memoria con guardado en disco.
- 4.6 y 4.7 se fusionaron después mediante PR #4 y PR #5. 4.8 se integró después por PR #6. Fase actual: **4.9**, rama `feat/knowledge-visibility`, pendiente de auditoría e integración.

### P1 — Prueba Android NDK temprana (imprescindible)

- Compilar CoreRPG mínimo con C++20 y un NDK/ABI Android soportados.
- Ejecutar una operación lógica desde una **aplicación Android independiente** en el dispositivo objetivo, sin MSI.
- Verificar C++ runtime, configuración de excepciones, enlazado, creación/consulta de entidades y resultados reproducibles.
- Evaluar separadamente `CoreRPGContentJson`: dependencias, acceso a paquetes dentro de APK, posibles ficheros extraídos o un adaptador de lectura. No trasladar API Android al Core.
- **Aceptación:** verificación real en Android; compilar sólo para Windows no acredita la plataforma.

**Ubicación temporal:** a continuación de 4.5 o paralela a 4.6, antes de expandir intensamente el runtime.

### P2 — Spike acotado CoreRPG + UE5 (conveniente, antes de integración masiva)

- Comprobar compilador, toolchain, enlace, política de excepciones/errores y ownership de memoria/hilos.
- Crear una entidad lógica, enlazarla a un Actor de prueba, descargar/destruir el Actor y comprobar la persistencia de identidad lógica.
- Mantener el experimento gráfico del puente de Star Trek **independiente** del cierre de CoreRPG; puede funcionar con datos simulados.
- **Aceptación:** la desaparición del Actor no reemite ni destruye EntityId, y UE5 no se convierte en dependencia del Core.

### P3 — Persistencia y simulación desacopladas de representación (4.7–4.10)

- Definir con consumidores reales cómo guardar identidad, revisión, contenido vivo, tiempo, eventos y horarios sin guardar todas las animaciones.
- Validar, cuando existan los sistemas, dos escenarios:
  - **Star Trek:** dotación lógica numerosa y sólo una minoría de NPC como actores 3D activos. Conservar turnos, labores y presencia cuando la cubierta no está cargada.
  - **Nimroel:** población, comercio y desplazamientos fuera de la escena 2D. Al volver a cargar, no regenerar los hechos.
- **Aceptación futura:** promoción/degradación de detalle mantiene identidades, hechos y continuidad. No fijar presupuestos de actores o rendimiento sin medidas.

### P4 — Contratos reutilizables de solicitudes, resultados y proyecciones (4.9–4.12)

- Mantener una fuente de verdad y validación tanto para acciones desde HUD como desde futuras superficies auxiliares.
- Proyectar únicamente datos autorizados por Knowledge & Visibility.
- Diseñar interfaces de intercambio pequeñas sólo cuando acciones/eventos de ambos juegos demuestren necesidades comunes; no crear un bus de eventos universal especulativo.

### P5 — RobDev Companion (aplazable; fuera del Core)

**Todavía no autorizado para implementación dentro del motor.**

1. Prototipo externo de web táctil y enlace Wi-Fi local; WebSocket como candidato.
2. Probar detección, emparejamiento/seguridad, reconexión y desconexión en caliente sin detener la partida.
3. Evaluar USB real sin exigir ADB o depuración USB permanente; elegir tecnología tras prueba.
4. Definir sesión, identidad de mundo (EntityId contextual), versiones y errores del protocolo, comandos idempotentes o protección contra duplicados y proyecciones sin secretos.
5. Demostrar primero lectura sincronizada y después solicitud real validada por la autoridad del juego.

**Criterio innegociable:** sin tablet y sin Companion disponible, Star Trek y Nimroel Windows mantienen todas las funciones y arrancan normalmente. La tablet no ejecuta segunda partida ni envía mutaciones directas a WorldState.

### P6 — Hosts y HUD de Nimroel por plataforma (obligatorio para entrega del juego)

- Windows: HUD completo 2D con teclado/ratón.
- Android nativo: HUD completo 2D táctil y partida **independiente** sin MSI.
- Windows con Companion: mismo HUD interno completo más pantalla adicional opcional.
- No confundir UI Android del juego con UI Companion. La sincronización de partidas Windows↔Android es una posible decisión futura distinta.

### P7 — Iniciativa, autoridad, encargos y delegación NPC (candidato aplazado)

**Estado:** necesidad multiuniverso justificada en Star Trek y Nimroel, **sin contrato ni implementación autorizados**. Documento de análisis: [RFC iniciativa NPC](proposals/npc-initiative-authority-delegation.md).

- Separar iniciativa de NPC, autoridad narrativa contextual, decisión de obedecer, disponibilidad de recursos y ejecución técnica autoritativa.
- **No implementarlo en 4.6**: las comprobaciones puras de hechos no son un sistema de órdenes o tareas.
- Revaluar una interfaz mínima después de Actions/Simulation 4.10, con Knowledge 4.9 y Persistence 4.7; considerar opción de sistema reutilizable separado y políticas específicas de cada juego.
- Exigir acciones NPC fuera de escena, revocación de delegaciones, trazabilidad sin revelar secretos y tolerancia a caída de IA remota. No implantar una jerarquía universal.

## Riesgos y decisiones que quedan abiertos

| Tema | Comprobación requerida | Evitar |
| --- | --- | --- |
| Portabilidad Android | NDK, STL, ABI, excepciones, app real | Dar por hecho soporte Android por existir C++20 |
| Archivos ContentJson/Android | Empaquetado APK y acceso a ficheros | Depender de rutas Windows o meter AssetManager en Core |
| Enlace Unreal | ABI, configuración C++, errores, propiedad de hilos | Acoplar WorldState a UObject/Actor |
| Personajes y LOD | Estado autoritativo antes de crear sprite/Actor | Igualar personaje con representación visible |
| Motor gráfico Nimroel | Evaluación futura de motor 2D Windows/Android | Imponer 2.5D, 3D o UE5 |
| Companion | Transporte USB/Wi-Fi, sesión, auth y control de conocimiento | Obligarlo, dar autoridad remota, exigir ADB |
| Rendimiento | Prueba real en MSI y Android | Inventar límites de actores o tamaños de mundo |

## Reglas de aceptación

1. Star Trek Windows funciona **sin Companion**.
2. Nimroel Windows funciona **sin Companion**.
3. Nimroel Android funciona **sin ordenador**, sin conexión local y sin Companion.
4. Una entidad lógica sobrevive a descargar una escena o su representación 2D/3D.
5. Todo comando, venga de HUD o de Companion, pasa por el mismo control de autoridad.
6. No se crean dos motores RPG ni se obliga a compartir motor gráfico.
7. CI Windows/Linux no sustituye las pruebas Android NDK y UE5.
8. No se reabre 4.5 para introducir redes, UI ni sistemas posteriores.

**Siguiente paso del Core:** auditar 4.9 en su PR Draft; integración sujeta a decisión humana. No se inicia 4.10. P1 Android NDK sigue siendo una verificación independiente.
