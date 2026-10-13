# ADR-0003 — Reservas poblacionales e identidades relacionadas

Estado: adoptada para implementación en 4.8, pendiente de auditoría de la PR.
Fecha: 2026-10-13. Precedente: ADR-0001; base `483ad13`, PR #5 integrada.

## Decisión y clasificación

| Capacidad | Nivel | Decisión |
| --- | --- | --- |
| Identidad runtime, revisión, contención | Core | Conservar contratos 4.5 |
| Asociación a DefinitionId existente | Core | Referencia estructural opcional e inmutable, no instanciación de payload |
| Relaciones dirigidas tipadas con integridad referencial | Core | Etiquetas uint64 estables por campaña, sin semántica de juego |
| Reserva agregada y emisión acotada de identidades | Core | Conteos explícitos, conservación y origen de individuos |
| Genealogía, hogares o modelos compartidos de organización | Candidato reusable | No implementar sin invariantes comunes de consumidores reales |
| Matrimonio, especie, profesión, rango, ciudadanía y mando | Universo/política | Fuera del Core; etiqueta no otorga autoridad |
| Representación activa, Actor, sprite y detalle visual | Host | No modifica existencia ni individualización |
| Arquetipos ricos y generación narrativa | Contenido/fase futura | El contrato 0.1.0 no define sus payloads |

## Entidades y relaciones

No hay ECS ni property bag. EntityId mantiene su identidad y WorldState es la
única autoridad. Una referencia opcional a DefinitionId se enlaza contra un
ContentCatalog validado; cambiar el significado de una entidad mediante
reasociación implícita se rechaza. Compartir valores inmutables en copias no
comparte mutaciones ni crea un mundo paralelo. La referencia no copia propiedades
ni convierte el envelope 0.1.0 en un arquetipo completo.

Una relación es (source EntityId, RelationKind, target EntityId). Los extremos
existen, el tipo es un uint64 no nulo y el triple es único. El orden numérico
canónico es source, kind, target. Las etiquetas las asigna el consumidor de
forma estable y documentada, nunca mediante std::hash, reloj o direcciones.
No se crea un registro de tipos o lenguaje de reglas preventivo.

Ciclos y autorrelaciones son válidos para el grafo genérico; prohibirlos en
parentesco o cadena de mando corresponde a una política específica, no a la
contención. No hay simetría, transitividad, rangos ni permisos implícitos.
Añadir/quitar un triple incrementa revisión una vez; duplicados y ausencia
producen no-op. Borrar una relación no borra entidades.

## Población

Una entidad existente puede anclar una reserva con total declarado y cantidad
latente. Latente significa unidades **sin EntityId todavía**, no personas
individualizadas fuera de cámara. Las unidades emitidas poseen IDs reales y
un origen inmutable (entidad → reserva). Pertenencia social u organizativa
actual se representa por relaciones separadas; origen no es contención,
residencia, familia ni pertenencia exclusiva a un grupo social.

La reserva se declara una vez. Repetir su total original es no-op incluso tras
individualizar; otro total se rechaza. No hay desindividualización, borrado ni
relleno que permita reemitir identidades históricas.

La materialización acepta cantidad y presupuesto explícitos (256 por defecto),
rechaza el lote entero si supera presupuesto, latencia, IDs o revisión. No
inventará rasgos: emite identidades y registra origen. Un lote publica N
creaciones elementales de manera atómica y aumenta revisión N veces, una por
identidad; create_entity() individual mantiene exactamente su contrato.
Cantidad cero es no-op después de validar la reserva. No consume RNG porque
no existe una elección de generación justificada por los datos actuales.

Conservación: total = latent + cantidad de entidades con ese origen. Los
grupos también son entidades lógicas; no se fija una jerarquía humano/colectivo.
El presupuesto acota la emisión, sin limitar la reserva a personas visibles.
Al descargar una escena, el Host sólo retira su representación.

## Persistencia y dependencias

Los datos nuevos exigen binario v2 explícito. V1 mantiene bytes y lectura.
Captura elige v1 sólo cuando representa sin pérdida todo el estado; en otro
caso v2. Un snapshot marcado v1 con campos nuevos se rechaza. V1 no migra
silenciosamente ni pierde datos. La carga de asociaciones a contenido requiere
el catálogo explícito para resolver referencias; no se serializa el catálogo.

Los contratos de snapshot se amplían junto con los campos de estado de los
incrementos 2/3. Hasta habilitar codec v2 en incremento 4, guardar ese estado
se rechaza explícitamente, nunca exporta un v1 incompleto. Esto reordena sólo
las declaraciones necesarias, no las garantías ni los incrementos funcionales.

## Comprobación multiuniverso

- Una reserva de dotación emite identidades que pueden asociarse a secciones
  mediante tipos elegidos por el consumidor; no conoce rango militar.
- Una reserva de habitantes emite identidades relacionadas con colectivos;
  no interpreta matrimonio, parentesco o residencia.
- Una colonia de unidades no humanas emite nodos y los asocia a agregados;
  no necesita especie, hogar, parentesco ni un personaje gráfico.

No se modelan agendas, IA, autoridad institucional, Knowledge ni Simulation.
La revisión futura debe examinar lifecycle con borrado, contratos de generación
reales y reglas específicas antes de ampliar estos datos.
