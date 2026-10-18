# ADR-0004 — Conocimiento recordado y proyecciones por observador

- Estado: aceptado; integrado en main mediante PR #7, merge `e7108e5`.
  Actualizado al iniciar 4.10; el contrato siguiente conserva las decisiones de 4.9.
- Fecha: 2026-10-18.

## Decisión y clasificación

WorldState conserva una única realidad y una tabla dispersa de hechos recordados
por observador. Un observador es un EntityId existente; no hay registro paralelo,
conocimiento propio automático ni permisos inferidos de relaciones o contención.
La necesidad común es detectar una entidad sin identificarla, o conocer sólo
algunos vínculos/secretos, tanto para exploración como para instrumentación.
Identidad, recuerdos y filtrado pertenecen al Core genérico. Sensores, alcance,
magia, confidencia, rumores y políticas sociales permanecen en consumidores;
no se justifica un sistema RPG reusable adicional.

Un FactKey contiene sujeto, tipo cerrado de hecho y, sólo para una relación,
tipo de relación y destino. Los tipos soportados son existencia, definición,
contenedor directo, relación exacta, reserva poblacional y origen de emisión.
No hay property bag, propiedades nuevas ni inferencia transitiva. Conocer una
entidad exige un hecho de existencia explícito y no revela ningún otro hecho.
Los valores recuerdan el estado al aprender, incluida ausencia conocida; falta
de registro significa desconocido. Un cambio secreto no refresca recuerdos.
Aprender otra vez actualiza explícitamente el recuerdo a la realidad actual.

Toda referencia expuesta debe tener existencia conocida para ese observador.
Aprender un hecho con extremo desconocido se rechaza; revocar existencia borra
sus hechos y los hechos que la referencian, sin revelar otros extremos. Revocar
un observador borra sus recuerdos. Mutación efectiva incrementa revisión global
una vez; no-op/rechazo no la cambia, ni consume RNG. Se prepara cualquier valor
o capacidad antes de publicar; fallos de asignación preservan el valor previo.
Orden canónico: observador, sujeto, tipo de hecho, tipo de relación, destino.

## Visibilidad y confianza

ObserverView es un valor separado, de sólo lectura, sin puntero al WorldState,
RNG, revisión global, último ID, totales globales ni diagnóstico de existencia
oculta. Enumera sólo sujetos autorizados. Buscar un ID oculto, inválido o ausente
produce el mismo resultado desconocido. Los IDs son opacos, no contadores de
entidades existentes: sus valores no autorizan consultas al mundo.

Visibility::project es una API **privilegiada**: recibe mundo, observador y una
lista explícita de concesiones temporales del caller de confianza. Combina
recuerdos con hechos actuales concedidos, sin mutar Knowledge. Las concesiones
no autorizan automáticamente extremos, definiciones ni todos los vínculos.
Existencia temporal debe concederse explícitamente para sujetos/extremos aún
desconocidos; el resultado no depende del orden ni duplicación de concesiones.
Una concesión inválida/no resoluble se omite sin errores distinguibles.

El servicio autentica y fija el observador y las concesiones; un consumidor no
confiable recibe exclusivamente ObserverView y no el builder ni WorldState.
Un Host con acceso al mundo, Persistence o consultas privilegiadas puede leer
secretos. Esta frontera C++ no aísla código hostil en el mismo proceso ni oculta
canales temporales, patrones de asignación o la procedencia numérica de IDs.
No se promete revocar copias ya entregadas; el Host reemplaza o retira vistas.

## Persistencia y alcance

V3 explícito conservará recuerdos, ausencias y separación de observadores.
V1/v2 conservan bytes y semántica y cargan Knowledge vacío. Valores recordados
pueden estar desactualizados respecto al mundo; se valida estructura, referencias,
orden y existencia conocida, no se los refresca al cargar. Definiciones conocidas
se resuelven con catálogo explícito como los bindings de 4.8. No se guardan
concesiones temporales ni proyecciones. Restaurar mundo y RNG sigue siendo atómico.

No se inicia percepción, Actions, Inspector, IA, red, Companion o simulación.
CoreRPG permanece C++20 portable y no depende de UE5, Godot, Android o servicios.
