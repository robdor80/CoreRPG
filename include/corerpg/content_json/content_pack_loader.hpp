#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "corerpg/content_json/content_document_decoder.hpp"
#include "corerpg/content_json/json_document_reader.hpp"
#include "corerpg/content_json/json_schema_validator.hpp"
#include "corerpg/content_pack_input.hpp"

namespace corerpg::content_json {

struct ContentPackLoadOptions final {
    std::size_t maximum_manifest_bytes = 16U * 1024U * 1024U;
    std::size_t maximum_resource_bytes = 64U * 1024U * 1024U;
};

enum class ContentPackLoadDiagnosticCode {
    pack_root_not_found,
    pack_root_not_directory,
    pack_root_resolution_error,
    manifest_not_found,
    manifest_not_regular_file,
    manifest_path_escape,
    resource_not_found,
    resource_not_regular_file,
    resource_path_escape,
    file_read_error,
};

class ContentPackLoadDiagnostic final {
public:
    ContentPackLoadDiagnostic(ContentPackLoadDiagnosticCode code,
                              std::filesystem::path path,
                              std::optional<std::string> resource_locator,
                              std::string message);

    [[nodiscard]] ContentPackLoadDiagnosticCode code() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::optional<std::string>& resource_locator() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;

private:
    ContentPackLoadDiagnosticCode code_;
    std::filesystem::path path_;
    std::optional<std::string> resource_locator_;
    std::string message_;
};

class ContentPackDocumentDiagnostic final {
public:
    ContentPackDocumentDiagnostic(std::filesystem::path path,
                                  std::optional<std::string> resource_locator,
                                  JsonDiagnostic diagnostic);
    ContentPackDocumentDiagnostic(std::filesystem::path path,
                                  std::optional<std::string> resource_locator,
                                  std::vector<JsonSchemaDiagnostic> diagnostics);
    ContentPackDocumentDiagnostic(std::filesystem::path path,
                                  std::optional<std::string> resource_locator,
                                  JsonMappingDiagnostic diagnostic);

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::optional<std::string>& resource_locator() const noexcept;
    [[nodiscard]] const JsonDiagnostic* json_diagnostic() const noexcept;
    [[nodiscard]] const std::vector<JsonSchemaDiagnostic>* schema_diagnostics() const noexcept;
    [[nodiscard]] const JsonMappingDiagnostic* mapping_diagnostic() const noexcept;

private:
    std::filesystem::path path_;
    std::optional<std::string> resource_locator_;
    std::variant<JsonDiagnostic, std::vector<JsonSchemaDiagnostic>, JsonMappingDiagnostic>
        diagnostic_;
};

class ContentPackLoadResult final {
public:
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const ContentPackInput* pack() const noexcept;
    [[nodiscard]] const ContentPackLoadDiagnostic* load_diagnostic() const noexcept;
    [[nodiscard]] const ContentPackDocumentDiagnostic* document_diagnostic() const noexcept;

private:
    friend ContentPackLoadResult load_content_pack(
        const std::filesystem::path& pack_root, ContentPackLoadOptions options);

    explicit ContentPackLoadResult(ContentPackInput pack);
    explicit ContentPackLoadResult(ContentPackLoadDiagnostic diagnostic);
    explicit ContentPackLoadResult(ContentPackDocumentDiagnostic diagnostic);

    std::variant<ContentPackInput, ContentPackLoadDiagnostic,
                 ContentPackDocumentDiagnostic>
        result_;
};

[[nodiscard]] ContentPackLoadResult load_content_pack(
    const std::filesystem::path& pack_root, ContentPackLoadOptions options = {});

}  // namespace corerpg::content_json
