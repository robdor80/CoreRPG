# CoreRPG — Roadmap vivo: fases, plataformas y Hosts

**Fecha de revisión:** 2026-10-10.  
**Referencias:** [Arquitectura](architecture.md), [ADR-0001](adr/0001-multi-universe-architecture.md), [ADR-0002](adr/0002-platforms-presentation-companion.md).

Este documento añade **criterios transversales** a la hoja de ruta incremental. No renumera fases ni atribuye al Core capacidades no implementadas. El Core seguirá siendo un único núcleo C++20 independiente de la tecnología de presentación.

## Estado de partida

- Fases **4.1–4.4** cerradas en la documentación histórica de CoreRPG.
- **4.5 — Generic World State:** ABI/API todavía en evolución. Incrementos 1 y 2 publicados en `feat/generic-world-state` (commits `07d154d` y `2a3da89`), con CI Windows/MSVC y Linux/GCC superada. Incremento 3 informado como terminado localmente pero **pendiente de revisión, commit, push, CI, PR, merge y acta de cierre**. No afirmar 4.5 cerrada antes de completar esos pasos.
- `main` de CoreRPG sigue en fase 4.4 al crear este documento. Evitar confundir estado de una rama y estado de `main`.
- Aún no están implementados: registro rico de personajes, horario/estado vivo completo, simulación fuera de escena, persistencia de partidas, módulos de Host, Companion y sus HUD.

## Plataformas y presentaciones fijadas

| Producto | Plataforma | Representación | Interfaz obligatoria |
| --- | --- | --- | --- |
| Star Trek | Windows/MSI | UE5, 3D | Teclado/ratón; jugable sin tablet |
| Nimroel Windows | Windows/MSI | Estrictamente 2D | HUD completo teclado/ratón |
| Nimroel Android | Android nativo / Galaxy Tab S9+ | Estrictamente 2D | HUD completo táctil, sin MSI ni red |
| RobDev Companion | Tablet conectada a juego Windows | UI auxiliar, no videojuego | Opcional, sin autoridad de partida |

**Invariante:** actor UE5 o representación 2D ausente NO significa entidad inexistente. No mantener un segundo WorldState de la campaña en la presentación.

## Fases funcionales: interpretación sin renumeración

| Fase | Alcance y nueva precaución |
| --- | --- |
| **4.5 Generic World State** | Cerrar identidad, revision, enumeración, contención y validaciones existentes; **sin** acoplar Android, UE5 o Companion. |
| **4.6 RNG + Rules** | RNG y reglas genéricas independientes de gráficos/entradas; no convertir procedimientos de sensores o magia en reglas hardcoded del Core. |
| **4.7 Persistence** | Restauración exacta de identidades, asignador, revision, vínculos y futuros hechos relevantes; contrato de guardado portable y versionado, sin rerollear identidades. |
| **4.8 Entidades/población/familias** | **Reevaluar alcance multiuniverso antes de implementar**: población latente, materialización, actividades y relaciones no deben introducir humanidad o familias como requisito universal. |
| **4.9 Knowledge & Visibility** | Proyecciones por observador. El Host/Companion no pueden recibir conocimiento oculto como datos visibles. |
| **4.10 Actions + Simulation** | Solicitudes estructuradas y resultados validados por autoridad única; la simulación off-screen deberá preservar causalidad e identidad. |
| **4.11 Inspector** | Observación no mutante del estado lógico; diagnóstico para Hosts y validación. |
| **4.12 Context Builder + AI** | Contextos autorizados; IA sin acceso directo de mutación. |
| **4.13 Primer Content Pack Nimroel** | Contenido real y validación de un consumidor, sin imponer un renderizador. |
| **4.14 Integración prevista con Unreal** | Vinculada específicamente al Host **Star Trek/UE5**. **No** obliga a Nimroel a usar Unreal. La integración de Nimroel 2D se planificará en paralelo como Host independiente, sin cambiar por ahora los números de fase. |
| **4.15 Vertical slice Treskal** | Demostración real Nimroel en 2D; pruebas del estado, la presentación y la continuidad. |

No significa que todos los conceptos de las fases posteriores pertenezcan al Core: aplicar siempre la clasificación Core / reusable / específico de ADR-0001.

## Hitos transversales y orden recomendado

### P0 — Cerrar fase 4.5 (inmediato al retomar Codex; imprescindible)

1. Revisar los **cuatro archivos locales** del Incremento 3: `docs/architecture.md`, `tests/CMakeLists.txt`, `tests/world_state_allocation_test.cpp` y el nuevo `tests/world_state_integration_test.cpp`.
2. Commit, push, CI de rama Windows/Linux, PR a `main`, merge, CI de `main` y acta histórica.
3. **No sobrescribir ni alterar el árbol local pendiente de Codex** desde la línea de trabajo documental.

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

**Siguiente paso del Core:** concluir el Incremento 3 de 4.5, integrar su PR y documentar el cierre. En paralelo o poco después, ejecutar P1 Android NDK.
