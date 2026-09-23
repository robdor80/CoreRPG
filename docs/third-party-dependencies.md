# Dependencias de terceros

## jsoncons

La biblioteca opcional `CoreRPGContentJson` utiliza actualmente
[jsoncons](https://github.com/danielaparker/jsoncons) como dependencia privada
para la integración del formato JSON de Content Packs.

- Versión: `v1.9.0`.
- Commit fijado: `bcb44594c50c495ee1e690602cdd71455942ad0e`.
- Licencia: Boost Software License 1.0 (`BSL-1.0`).

La dependencia se obtiene durante la configuración del build únicamente cuando
`CORERPG_BUILD_CONTENT_JSON` está habilitada. No forma parte de `CoreRPG` ni
introduce acceso de red en runtime.

Una futura distribución que incluya jsoncons deberá conservar el aviso de
licencia correspondiente proporcionado por el proyecto upstream.
