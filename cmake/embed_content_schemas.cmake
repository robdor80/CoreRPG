cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS OUTPUT_FILE TYPES_SCHEMA MANIFEST_SCHEMA RESOURCE_SCHEMA)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "Missing required variable: ${required_variable}")
    endif()
endforeach()

get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")

file(WRITE "${OUTPUT_FILE}" "#pragma once\n\n#include <cstddef>\n\nnamespace corerpg::content_json::embedded {\n\n")

function(append_schema symbol source_file)
    file(READ "${source_file}" schema_hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " schema_bytes "${schema_hex}")
    file(APPEND "${OUTPUT_FILE}"
        "inline constexpr unsigned char ${symbol}[] = {${schema_bytes}};\n"
        "inline constexpr std::size_t ${symbol}_size = sizeof(${symbol});\n\n"
    )
endfunction()

append_schema(types_schema "${TYPES_SCHEMA}")
append_schema(content_pack_manifest_schema "${MANIFEST_SCHEMA}")
append_schema(resource_definition_schema "${RESOURCE_SCHEMA}")

file(APPEND "${OUTPUT_FILE}" "}  // namespace corerpg::content_json::embedded\n")
