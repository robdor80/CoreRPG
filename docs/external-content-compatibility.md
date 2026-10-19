# Compatibilidad de publicaciones externas y guardados

Aplicada en 4.13 sin modificar Content Contract 0.1.0 ni CRPGSAVE v1/v2/v3.
El catálogo conserva sólo ID/kind y no retiene manifests o payloads. Restore
resuelve referencias, pero no puede detectar por sí solo un cambio semántico de
un pack que mantenga sus DefinitionId. Esa detección pertenece al adaptador/Host.

## Política de publicación y restauración

1. Seleccionar una publicación de confianza de cada pack. El Host comprueba
   bytes/schema/referencias y su huella antes de producir el catálogo desde esos
   mismos bytes. Mantener bundle/catálogo inmutables durante la operación.
2. Conservar al crear/guardar la partida metadatos externos: PackId, pack_version,
   identidad del conjunto ID/kind y huella de la publicación. Los packs se ordenan
   por PackId, sin duplicados. Conservar también identidad/huella del contexto de
   generación inicial: semilla, ID estable del asentamiento, reglas, generador y
   fixture/grafo regional versionado. No duplicar el estado vivo en esos metadatos.
3. Al restaurar, verificar la publicación instalada y comparar con los metadatos
   originales **antes** de llamar a Persistence::load. Exigir igualdad exacta en
   esta primera política; un cambio se rechaza si no existe migración explícita.
4. Sólo tras esa comprobación llamar a load con ContentCatalog explícito.
   Su rechazo o excepción conserva conjuntamente WorldState y RandomStreams.
   Una definición ausente se rechaza incluso si el adaptador se equivocó antes.
5. La restauración nunca ejecuta la receta inicial, materializa población de nuevo
   ni rellena hogares/servicios. Continuar desde las tablas y streams persistidos.

Conservar la publicación antigua es una forma válida de continuar. Una versión
SemVer más alta no autoriza por sí misma cargar ni migrar. No hay migrador
automático de canon o instancias ni compatibilidad futura implícita. Los datos
externos han de formar parte del guardado gestionado por el Host con durabilidad y
coherencia; el núcleo sólo proporciona bytes. Checksum/hash detectan diferencias,
no acreditan autenticidad del publicador ni protegen frente a edición deliberada.

## Ejemplo ejecutable de la frontera

[examples/external_content_restore.hpp](../examples/external_content_restore.hpp)
aporta valores externos de metadatos y la comparación previa. Es un **ejemplo
neutral del adaptador**, no nueva API del Core ni un sistema de mods. El caller
aporta `installed` desde un bundle ya verificado y el catálogo que se construyó
con él. La función no descarga/hashea archivos, autentica publicadores ni traduce
metadatos desde JSON; esos pasos son del Host. La validación propia del ejemplo
limita a 128 packs y textos de identificación; no reconstruye tipos poseídos.
La gramática y tipos fuertes se validan en el decoder/catálogo del preflight.
Comparar metadatos ya verificados no asigna memoria ni añade otra autoridad.
Estos límites son recursos técnicos, no reglas de juego.

Las dos pruebas neutrales se ejecutan con ContentJson desactivado; comprueban que
IDs idénticos no autorizan versión/huella distinta y que el camino aceptado usa
Persistence real, incluidos sus límites y publicación atómica. La integración
externa 4.13 conecta el preflight real Python (hashes/schema/canon/fuentes) con
ese ejemplo y el catálogo cargado desde el mismo bundle, y prueba más ejes de
incompatibilidad, memoria parcial, estado RNG y continuidad de entidades.
Un Host C++ privilegiado sigue siendo responsable de usar la publicación que
validó. No existe sandbox frente a C++ hostil dentro del proceso.

## Aplicación al primer pack de Nimroel

El contrato específico reside en Videojuego_Nimroel/ContentPacks/treskal-villages.
`content-lock.json` contiene SHA-256 de cada documento y de su mapa canónico;
`definition_set_sha256` fija pares ID/kind. El algoritmo canónico es UTF-8,
claves ordenadas, indentación de dos espacios, LF y LF final. El fixture técnico
está incluido en el mapa: sus cambios también invalidan la huella de publicación.
La huella del fixture sirve de contexto inicial externo, sin añadir nada al codec.
La fuente de semilla 413001 es explícita; no es un algoritmo de generación regional.

Nimroel es público y CoreRPG privado: la CI pública valida sus schemas propios y
restricciones de esta publicación sin copiar schemas privados ni recibir tokens
adicionales. La CI Core valida además los schemas Core originales, tipos fuertes
y catálogo, usando una revisión pública de Nimroel fijada por SHA. El juego
instalado no necesita red. La preparación de CI no forma parte de la biblioteca.
