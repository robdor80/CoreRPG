# ADR-0002 — Plataformas, presentación y Companion opcional

**Estado:** ACEPTADA para las decisiones de producto y las fronteras arquitectónicas indicadas. Los detalles técnicos expresamente abiertos requieren decisiones posteriores.  
**Fecha:** 2026-10-10.  
**Complementa:** [ADR-0001](0001-multi-universe-architecture.md).  
**Ejecución:** [Roadmap](../roadmap.md).

## Contexto

CoreRPG es un núcleo lógico C++20, portable, determinista y autoritativo. Los Content Packs describen contenido y los Hosts adaptan presentación y entradas. Las plataformas definitivas de los consumidores de referencia concretan esta separación, pero **no requieren rehacer 4.1–4.5 ni cambiar ahora sus APIs**. Esta ADR registra decisiones; no declara implementados sistemas futuros.

## Decisiones estables

### Star Trek

- Videojuego **3D en Unreal Engine 5** para **Windows PC**; MSI Raider GE78 HX como máquina objetivo actual, teclado y ratón principales. Android nativo queda fuera del objetivo actual de Star Trek.
- La nave y sus interiores 3D (puente, consolas, ingeniería, camarotes, pasillos y dependencias) son escenarios recurrentes. Optimización y alcance visual deberán validarse con prototipos.
- Los tripulantes son entidades lógicas persistentes **independientes de los actores 3D**. Una nave puede tener, por ejemplo, 300 personas registradas sin representar 300 actores simultáneos. El número es ilustrativo, no una constante del Core.
- Distinguir identidad, destino, empleo, guardia, estación de servicio, residencia, presencia efectiva, actividad y actor visible; no representar todo esto mediante una sola relación de contención.
- Rangos, naves, turnos y operaciones son específicas de Star Trek, apoyadas en capacidades comunes cuando resulte justificado.

### Nimroel

- Videojuego **estrictamente 2D**; no se requiere 2.5D, 3D isométrico ni Unreal.
- Ejecución **nativa e independiente** en Windows y Android (Samsung Galaxy Tab S9+ como referencia). Android no necesita MSI encendido, conexión local ni Companion.
- Windows tendrá HUD 2D completo para teclado/ratón; Android, HUD táctil 2D completo.
- El motor gráfico 2D concreto permanece **sin elegir**. La escala del mundo y la persistencia de NPC no dependen del nivel de detalle de la presentación.

### Núcleo y Hosts

- **Un núcleo lógico; varios Hosts gráficos.** CoreRPG no depende de UE5, mallas, sprites, animaciones, widgets, pantalla táctil, Windows, Android, socket o UI.
- Un actor visual puede crearse/desaparecer sin crear o destruir por ello una entidad lógica; los hechos autoritativos y la identidad sobreviven a la descarga de escenas.
- La contención directa de 4.5 es estructural y opcional, **no** posición 2D/3D, posesión, propiedad, residencia o puesto de trabajo.
- Los Hosts solicitan acciones, muestran resultados y gestionan entradas; no constituyen un segundo WorldState autoritativo.
- Portabilidad por diseño **no equivale** a portabilidad comprobada. Es obligatoria una validación Android NDK temprana para Nimroel y un spike específico de integración C++/UE5 para Star Trek, sin arrastrar esas dependencias al Core.
- Las mecánicas específicas y sistemas reutilizables se clasifican conforme a ADR-0001, no por semejanzas nominales.

### RobDev Companion

- Capacidad **futura**, compartible por ambos juegos **cuando se ejecutan en el MSI**; es una integración exterior a CoreRPG.
- Tablet auxiliar para consultar **proyecciones autorizadas** y enviar **solicitudes** al Host principal. No ejecuta segunda partida ni escribe directamente en WorldState.
- **Regla obligatoria:** Companion es extra, jamás requisito. Ambos juegos deben arrancar y ofrecer el 100 % de funciones en sus HUD sin buscar tablet o conexión. Una desconexión no interrumpe la partida; reconexión/resincronización deseables.
- Wi-Fi local y USB son candidatos. WebSocket/UI web son hipótesis de diseño; **ADB no será requisito permanente**.
- Antes de habilitar órdenes remotas: sesión, emparejamiento/autorización, versión de protocolo, resultados y errores, protección ante duplicados/reconexión y filtrado de datos por Knowledge/Visibility. Nunca revelar la verdad oculta del universo al cliente.
- **No** introducir servidores, red, protocolo Companion ni UI dentro del Core ahora.

## Modos de ejecución obligatorios

| Modo | Realidad autoritativa | Presentación | Companion |
| --- | --- | --- | --- |
| Star Trek Windows | Partida en MSI | UE5 3D y controles completos | Opcional |
| Nimroel Windows | Partida en MSI | 2D, HUD completo teclado/ratón | Opcional |
| Nimroel Android | Partida independiente en tablet | 2D, HUD táctil completo | No es necesario |

Nimroel Android nativo y Companion **son dos modos diferentes**. No se presume sincronización automática de partidas entre Windows y Android.

## Estado comprobado y riesgos de integración

- CoreRPG ofrece identidad runtime contextual, emisión y enumeración de EntityId, revisión y contención opcional en la rama de 4.5. RNG/RollAction son infraestructuras mínimas; ContentCatalog es declarativo.
- No están implementados registros de dominio completos, puente general DefinitionId→EntityId, población, horarios, reglas/acciones generales, simulación, guardado, HUD ni Companion.
- CMake compila CoreRPG C++20; la CI actual valida Windows/MSVC y Linux/GCC, **no Android/NDK ni UE5**.
- La carga física ContentJson es opcional y usa filesystem; recursos internos de APK pueden requerir otro mecanismo o extracción. No insertar AssetManager en el Core.
- En UE5 se deberá verificar compilador/ABI, políticas de excepciones, memoria y propiedad de hilos sin acoplar WorldState a UObject/Actor.

## Decisiones pendientes

Motor 2D de Nimroel; implementación concreta de adaptadores UE5/Android; empaquetado de Content Packs Android; formato/ciclo de guardado; datos de población/turnos y LOD; transporte real USB sin depuración, seguridad/protocolo y UX de Companion. **Ninguna elección se realiza aquí por anticipado**.

## Seguimiento y control

[docs/roadmap.md](../roadmap.md) fija hitos de prueba. Esta ADR no altera fases cerradas, no reabre 4.5, no autoriza un framework de plugins ni adelanta Companion.

**Principio:** identidad y realidad independientes del renderizador; Companion opcional y no autoritativo.
