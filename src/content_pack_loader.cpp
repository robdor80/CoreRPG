#include "corerpg/content_json/content_pack_loader.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace corerpg::content_json {
namespace {

using FileReadResult =
    std::variant<std::vector<std::byte>, ContentPackLoadDiagnostic, JsonDiagnostic>;

[[nodiscard]] ContentPackLoadDiagnostic load_error(
    ContentPackLoadDiagnosticCode code, const std::filesystem::path& path,
    std::optional<std::string> resource_locator, std::string message) {
    return ContentPackLoadDiagnostic(code, path, std::move(resource_locator),
                                     std::move(message));
}

[[nodiscard]] FileReadResult read_file_bytes(
    const std::filesystem::path& path, std::size_t maximum_bytes,
    const std::optional<std::string>& resource_locator) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return load_error(ContentPackLoadDiagnosticCode::file_read_error, path,
                          resource_locator, "Cannot open file for binary reading");
    }

    std::vector<std::byte> bytes;
    bytes.reserve(maximum_bytes < 64U * 1024U ? maximum_bytes : 64U * 1024U);
    std::array<char, 8192U> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            const auto byte_count = static_cast<std::size_t>(count);
            if (byte_count > maximum_bytes - bytes.size()) {
                return JsonDiagnostic(JsonDiagnosticCode::document_too_large,
                                      std::nullopt, std::nullopt, std::nullopt);
            }
            const auto* begin = reinterpret_cast<const std::byte*>(buffer.data());
            bytes.insert(bytes.end(), begin, begin + byte_count);
        }
    }

    if (input.bad() || (!input.eof() && input.fail())) {
        return load_error(ContentPackLoadDiagnosticCode::file_read_error, path,
                          resource_locator, "Error while reading file bytes");
    }
    return bytes;
}

[[nodiscard]] bool is_strict_descendant(const std::filesystem::path& root,
                                        const std::filesystem::path& candidate,
                                        std::error_code& error) {
    const auto root_component_count =
        static_cast<std::size_t>(std::distance(root.begin(), root.end()));
    auto candidate_component = candidate.begin();
    std::filesystem::path candidate_prefix;
    for (std::size_t index = 0; index < root_component_count; ++index) {
        if (candidate_component == candidate.end()) {
            return false;
        }
        candidate_prefix /= *candidate_component;
        ++candidate_component;
    }
    if (candidate_component == candidate.end()) {
        return false;
    }
    return std::filesystem::equivalent(root, candidate_prefix, error);
}

[[nodiscard]] bool locator_is_defensively_relative(std::string_view locator) {
    const std::filesystem::path path(locator);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory() ||
        locator.find('\\') != std::string_view::npos ||
        locator.find(':') != std::string_view::npos) {
        return false;
    }
    for (const auto& component : path) {
        if (component == "." || component == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<ContentPackLoadDiagnostic> require_regular_file(
    const std::filesystem::path& path, ContentPackLoadDiagnosticCode not_found_code,
    ContentPackLoadDiagnosticCode not_regular_code,
    const std::optional<std::string>& resource_locator) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return load_error(ContentPackLoadDiagnosticCode::file_read_error, path,
                          resource_locator, "Cannot inspect filesystem entry");
    }
    if (!exists) {
        return load_error(not_found_code, path, resource_locator,
                          "Required file does not exist");
    }
    const bool regular = std::filesystem::is_regular_file(path, error);
    if (error) {
        return load_error(ContentPackLoadDiagnosticCode::file_read_error, path,
                          resource_locator, "Cannot inspect filesystem entry type");
    }
    if (!regular) {
        return load_error(not_regular_code, path, resource_locator,
                          "Required entry is not a regular file");
    }
    return std::nullopt;
}

[[nodiscard]] ContentPackDocumentDiagnostic json_read_failure(
    const std::filesystem::path& path, std::optional<std::string> resource_locator,
    const JsonDiagnostic& diagnostic) {
    return ContentPackDocumentDiagnostic(path, std::move(resource_locator), diagnostic);
}

}  // namespace

ContentPackLoadDiagnostic::ContentPackLoadDiagnostic(
    ContentPackLoadDiagnosticCode code, std::filesystem::path path,
    std::optional<std::string> resource_locator, std::string message)
    : code_(code),
      path_(std::move(path)),
      resource_locator_(std::move(resource_locator)),
      message_(std::move(message)) {}

ContentPackLoadDiagnosticCode ContentPackLoadDiagnostic::code() const noexcept {
    return code_;
}

const std::filesystem::path& ContentPackLoadDiagnostic::path() const noexcept {
    return path_;
}

const std::optional<std::string>&
ContentPackLoadDiagnostic::resource_locator() const noexcept {
    return resource_locator_;
}

const std::string& ContentPackLoadDiagnostic::message() const noexcept {
    return message_;
}

ContentPackDocumentDiagnostic::ContentPackDocumentDiagnostic(
    std::filesystem::path path, std::optional<std::string> resource_locator,
    JsonDiagnostic diagnostic)
    : path_(std::move(path)),
      resource_locator_(std::move(resource_locator)),
      diagnostic_(std::move(diagnostic)) {}

ContentPackDocumentDiagnostic::ContentPackDocumentDiagnostic(
    std::filesystem::path path, std::optional<std::string> resource_locator,
    std::vector<JsonSchemaDiagnostic> diagnostics)
    : path_(std::move(path)),
      resource_locator_(std::move(resource_locator)),
      diagnostic_(std::move(diagnostics)) {}

ContentPackDocumentDiagnostic::ContentPackDocumentDiagnostic(
    std::filesystem::path path, std::optional<std::string> resource_locator,
    JsonMappingDiagnostic diagnostic)
    : path_(std::move(path)),
      resource_locator_(std::move(resource_locator)),
      diagnostic_(std::move(diagnostic)) {}

const std::filesystem::path& ContentPackDocumentDiagnostic::path() const noexcept {
    return path_;
}

const std::optional<std::string>&
ContentPackDocumentDiagnostic::resource_locator() const noexcept {
    return resource_locator_;
}

const JsonDiagnostic* ContentPackDocumentDiagnostic::json_diagnostic() const noexcept {
    return std::get_if<JsonDiagnostic>(&diagnostic_);
}

const std::vector<JsonSchemaDiagnostic>*
ContentPackDocumentDiagnostic::schema_diagnostics() const noexcept {
    return std::get_if<std::vector<JsonSchemaDiagnostic>>(&diagnostic_);
}

const JsonMappingDiagnostic*
ContentPackDocumentDiagnostic::mapping_diagnostic() const noexcept {
    return std::get_if<JsonMappingDiagnostic>(&diagnostic_);
}

ContentPackLoadResult::ContentPackLoadResult(ContentPackInput pack)
    : result_(std::move(pack)) {}

ContentPackLoadResult::ContentPackLoadResult(ContentPackLoadDiagnostic diagnostic)
    : result_(std::move(diagnostic)) {}

ContentPackLoadResult::ContentPackLoadResult(ContentPackDocumentDiagnostic diagnostic)
    : result_(std::move(diagnostic)) {}

bool ContentPackLoadResult::succeeded() const noexcept {
    return std::holds_alternative<ContentPackInput>(result_);
}

const ContentPackInput* ContentPackLoadResult::pack() const noexcept {
    return std::get_if<ContentPackInput>(&result_);
}

const ContentPackLoadDiagnostic* ContentPackLoadResult::load_diagnostic() const noexcept {
    return std::get_if<ContentPackLoadDiagnostic>(&result_);
}

const ContentPackDocumentDiagnostic*
ContentPackLoadResult::document_diagnostic() const noexcept {
    return std::get_if<ContentPackDocumentDiagnostic>(&result_);
}

ContentPackLoadResult load_content_pack(const std::filesystem::path& pack_root,
                                        ContentPackLoadOptions options) {
    std::error_code error;
    const bool root_exists = std::filesystem::exists(pack_root, error);
    if (error) {
        return ContentPackLoadResult(load_error(
            ContentPackLoadDiagnosticCode::pack_root_resolution_error, pack_root,
            std::nullopt, "Cannot inspect Content Pack root"));
    }
    if (!root_exists) {
        return ContentPackLoadResult(load_error(
            ContentPackLoadDiagnosticCode::pack_root_not_found, pack_root, std::nullopt,
            "Content Pack root does not exist"));
    }
    const bool root_is_directory = std::filesystem::is_directory(pack_root, error);
    if (error) {
        return ContentPackLoadResult(load_error(
            ContentPackLoadDiagnosticCode::pack_root_resolution_error, pack_root,
            std::nullopt, "Cannot inspect Content Pack root type"));
    }
    if (!root_is_directory) {
        return ContentPackLoadResult(load_error(
            ContentPackLoadDiagnosticCode::pack_root_not_directory, pack_root,
            std::nullopt, "Content Pack root is not a directory"));
    }

    const auto canonical_root = std::filesystem::canonical(pack_root, error);
    if (error) {
        return ContentPackLoadResult(load_error(
            ContentPackLoadDiagnosticCode::pack_root_resolution_error, pack_root,
            std::nullopt, "Cannot canonicalize Content Pack root"));
    }

    const auto manifest_candidate = canonical_root / "manifest.json";
    if (auto diagnostic = require_regular_file(
            manifest_candidate, ContentPackLoadDiagnosticCode::manifest_not_found,
            ContentPackLoadDiagnosticCode::manifest_not_regular_file, std::nullopt);
        diagnostic.has_value()) {
        return ContentPackLoadResult(std::move(*diagnostic));
    }
    const auto manifest_path = std::filesystem::canonical(manifest_candidate, error);
    if (error) {
        return ContentPackLoadResult(load_error(
            ContentPackLoadDiagnosticCode::file_read_error, manifest_candidate,
            std::nullopt, "Cannot canonicalize manifest path"));
    }
    if (!is_strict_descendant(canonical_root, manifest_path, error) || error) {
        return ContentPackLoadResult(load_error(
            error ? ContentPackLoadDiagnosticCode::file_read_error
                  : ContentPackLoadDiagnosticCode::manifest_path_escape,
            manifest_path, std::nullopt,
            error ? "Cannot compare manifest path with Content Pack root"
                  : "Manifest resolves outside Content Pack root"));
    }

    auto manifest_bytes = read_file_bytes(manifest_path, options.maximum_manifest_bytes,
                                          std::nullopt);
    if (const auto* diagnostic = std::get_if<ContentPackLoadDiagnostic>(&manifest_bytes)) {
        return ContentPackLoadResult(*diagnostic);
    }
    if (const auto* diagnostic = std::get_if<JsonDiagnostic>(&manifest_bytes)) {
        return ContentPackLoadResult(
            json_read_failure(manifest_path, std::nullopt, *diagnostic));
    }
    auto manifest_read = read_json_document(
        std::span<const std::byte>(std::get<std::vector<std::byte>>(manifest_bytes)),
        JsonReaderOptions{options.maximum_manifest_bytes});
    if (!manifest_read.succeeded()) {
        return ContentPackLoadResult(json_read_failure(
            manifest_path, std::nullopt, *manifest_read.diagnostic()));
    }
    const auto manifest_decode =
        decode_content_pack_manifest(*manifest_read.document());
    if (manifest_decode.schema_diagnostics() != nullptr) {
        return ContentPackLoadResult(ContentPackDocumentDiagnostic(
            manifest_path, std::nullopt, *manifest_decode.schema_diagnostics()));
    }
    if (manifest_decode.mapping_diagnostic() != nullptr) {
        return ContentPackLoadResult(ContentPackDocumentDiagnostic(
            manifest_path, std::nullopt, *manifest_decode.mapping_diagnostic()));
    }

    const auto& decoded_manifest = *manifest_decode.decoded_manifest();
    std::vector<ContentDefinitionInput> definitions;
    definitions.reserve(decoded_manifest.resource_locators().size());
    for (const auto& locator : decoded_manifest.resource_locators()) {
        const std::optional<std::string> resource_locator(locator);
        if (!locator_is_defensively_relative(locator)) {
            return ContentPackLoadResult(load_error(
                ContentPackLoadDiagnosticCode::resource_path_escape,
                canonical_root / std::filesystem::path(locator), resource_locator,
                "Resource locator is not a safe relative path"));
        }

        const auto resource_candidate =
            canonical_root / std::filesystem::path(locator);
        if (auto diagnostic = require_regular_file(
                resource_candidate, ContentPackLoadDiagnosticCode::resource_not_found,
                ContentPackLoadDiagnosticCode::resource_not_regular_file,
                resource_locator);
            diagnostic.has_value()) {
            return ContentPackLoadResult(std::move(*diagnostic));
        }
        const auto resource_path = std::filesystem::canonical(resource_candidate, error);
        if (error) {
            return ContentPackLoadResult(load_error(
                ContentPackLoadDiagnosticCode::file_read_error, resource_candidate,
                resource_locator, "Cannot canonicalize resource path"));
        }
        if (!is_strict_descendant(canonical_root, resource_path, error) || error) {
            return ContentPackLoadResult(load_error(
                error ? ContentPackLoadDiagnosticCode::file_read_error
                      : ContentPackLoadDiagnosticCode::resource_path_escape,
                resource_path, resource_locator,
                error ? "Cannot compare resource path with Content Pack root"
                      : "Resource resolves outside Content Pack root"));
        }

        auto resource_bytes = read_file_bytes(
            resource_path, options.maximum_resource_bytes, resource_locator);
        if (const auto* diagnostic =
                std::get_if<ContentPackLoadDiagnostic>(&resource_bytes)) {
            return ContentPackLoadResult(*diagnostic);
        }
        if (const auto* diagnostic = std::get_if<JsonDiagnostic>(&resource_bytes)) {
            return ContentPackLoadResult(
                json_read_failure(resource_path, resource_locator, *diagnostic));
        }
        auto resource_read = read_json_document(
            std::span<const std::byte>(std::get<std::vector<std::byte>>(resource_bytes)),
            JsonReaderOptions{options.maximum_resource_bytes});
        if (!resource_read.succeeded()) {
            return ContentPackLoadResult(json_read_failure(
                resource_path, resource_locator, *resource_read.diagnostic()));
        }
        const auto resource_decode =
            decode_resource_definition(*resource_read.document());
        if (resource_decode.schema_diagnostics() != nullptr) {
            return ContentPackLoadResult(ContentPackDocumentDiagnostic(
                resource_path, resource_locator,
                *resource_decode.schema_diagnostics()));
        }
        if (resource_decode.mapping_diagnostic() != nullptr) {
            return ContentPackLoadResult(ContentPackDocumentDiagnostic(
                resource_path, resource_locator,
                *resource_decode.mapping_diagnostic()));
        }
        definitions.push_back(*resource_decode.definition());
    }

    return ContentPackLoadResult(
        ContentPackInput(decoded_manifest.manifest(), std::move(definitions)));
}

}  // namespace corerpg::content_json
