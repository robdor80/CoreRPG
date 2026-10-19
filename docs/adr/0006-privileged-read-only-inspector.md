# ADR-0006 — Inspector técnico privilegiado de solo lectura

- Estado: integrada en main por PR #9, merge `4bda9d3` (2026-10-19).
  Estado actualizado al iniciar 4.12; se conserva el contrato histórico.
- Fecha: 2026-10-19.
- Base: `7d497b99802773cbb46c06eaee71a2892b772a05`, PR #8 fusionada.

## Decisión vinculante

Inspector es exclusivamente técnico, privilegiado y de solo lectura.
**ObserverView sigue siendo la única vía de información autorizada hacia
jugadores, NPC, diálogo e IA.** Ni informes internos, ni revisiones, ni resultados
de acciones pueden entregarse crudos a esos sistemas o a sus prompts.
El Host controla acceso y autenticación; esta API C++ no aísla código hostil en
el mismo proceso. No hay permisos ficticios mediante tokens construibles.

## Contrato mínimo

Inspector toma prestado un `const WorldState&` existente; no posee otra realidad,
no acepta temporales y nunca conserva un catálogo o resultado de acción.
Consulta metadatos, un resumen de entidad y su binding, resolución opcional en
catálogo explícito, entidades, contención, relaciones dirigidas/tipadas,
reservas, origen poblacional y recuerdos persistidos. No infiere parentesco,
visibilidad, percepción, autorización ni verdad actual a partir del recuerdo.

Las colecciones usan offset y límite (máximo público 256), orden canónico y
páginas prestadas sin asignación. Las entidades implícitas se representan por
un intervalo contiguo, sin materializar el mundo. Cada página indica revisión,
siguiente offset y estado; una revisión esperada distinta devuelve página vacía.
Límite cero o excesivo se rechaza, offset al final/fuera devuelve fin vacío.
Las consultas individuales distinguen ausencia de entidad de campos opcionales.

El Host excluye mutaciones concurrentes durante **toda utilización** de referencias
y spans. Por contrato se descartan todas las vistas prestadas antes de cualquier
mutación, asignación, move, restore o destrucción del mundo/catálogo, aunque
alguna referencia pudiera sobrevivir físicamente. Una consulta no fija snapshot:
entre páginas el Host usa revisión esperada o impide mutaciones. La revisión
no identifica sesión/mundo y no detecta restauración/asignación con igual revisión;
el Host reinicia su recorrido tras reemplazar el mundo. No hay locks ni cursores.

## Diagnóstico y recursos

La validación estructural de Persistence se comparte internamente con Inspector:
identidades/revisión, orden/unicidad, extremos, contención acíclica, bindings,
reservas/origen y estructura/cierre de conocimiento. Se informa el primer fallo
en orden fijo, con código `PersistenceStatus` existente y severidad; no hay lista
sin límite ni reparación. Un recuerdo antiguo válido no es advertencia ni error.
La comprobación del mundo vivo no clona ni serializa el mundo ni necesita RNG.
Una variante sobre snapshot candidato permite explicar rechazos de persistencia,
sin restaurarlo. Se mantienen bytes, versiones y precedencia de validación.

El scratch de conteo/coloración tiene presupuesto explícito; sobrepasarlo o
fallar la asignación produce diagnóstico incompleto de recursos, nunca una
certificación de validez. Se acota por registros dispersos, no por último ID.
El coste es lineal/logarítmico en tablas dispersas; no hay recorrido recursivo.
No se valida memoria C++ arbitrariamente corrupta, sólo valores estructurales.

## Acciones, consumidores y límites

ActionResult ya es un valor técnico pequeño, tipado y completo. No duplicarlo
ni añadir un segundo formato. El Host conserva explícitamente los resultados
que quiera examinar y usa Inspector para consultar entidades/revisiones al
retorno de process/step. Sólo el prefijo `processed` de Simulation es definitivo.
Una revisión coincidente no prueba que un resultado pertenezca al mundo actual;
no hay explicación causal histórica reconstruible a partir del estado presente.

Nimroel necesita depurar entidades fuera de escena 2D y recuerdos privados;
Star Trek necesita contrastar universo real y proyecciones autorizadas. Inspector
no implementa diagnósticos de sensores, permisos de computadora, cadena de mando
ni registros operativos. Esas reglas pertenecen a sus consumidores.
No hay dependencias de motores, HTTP, proveedores IA, base de datos, historial,
replay, panel, scheduler, puente nativo ni Context Builder 4.12.

## Revisión de confidencialidad de I1

Sólo el Host técnico que ya posee WorldState puede construir Inspector. No se
modifica ObserverView/VisibleFact/Visibility ni se añade conversión hacia ellos,
concesiones automáticas, endpoint remoto o registro en sistemas jugables.
Los nuevos valores están documentados como privilegiados. Las regresiones
compararán proyecciones antes/después de inspeccionar secretos y resultados.
