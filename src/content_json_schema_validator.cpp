#include "corerpg/content_json/json_schema_validator.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <jsoncons/json.hpp>
#include <jsoncons/utility/uri.hpp>
#include <jsoncons_ext/jsonschema/draft202012/schema_draft202012.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include "corerpg/content_json/embedded_content_schemas.hpp"
#include "content_json_document_internal.hpp"

namespace corerpg::content_json {
namespace {

namespace jsonschema = jsoncons::jsonschema;

constexpr std::string_view types_schema_uri =
    "https://robdor80.github.io/CoreRPG/schemas/content/0.1.0/types.schema.json";
constexpr std::string_view manifest_schema_uri =
    "https://robdor80.github.io/CoreRPG/schemas/content/0.1.0/"
    "content-pack-manifest.schema.json";
constexpr std::string_view resource_schema_uri =
    "https://robdor80.github.io/CoreRPG/schemas/content/0.1.0/"
    "resource-definition.schema.json";

using CompiledSchema = jsonschema::json_schema<jsoncons::json>;

[[nodiscard]] std::span<const std::byte> as_bytes(const unsigned char* data,
                                                  std::size_t size) noexcept {
    return {reinterpret_cast<const std::byte*>(data), size};
}

[[nodiscard]] JsonSchemaDiagnostic configuration_diagnostic(std::string message) {
    return JsonSchemaDiagnostic(JsonSchemaDiagnosticCode::schema_configuration_error, {}, {},
                                {}, {}, std::move(message));
}

void append_validation_message(std::vector<JsonSchemaDiagnostic>& diagnostics,
                               const jsonschema::validation_message& message) {
    diagnostics.emplace_back(
        JsonSchemaDiagnosticCode::schema_violation,
        message.instance_location().string(), message.schema_location().base().string(),
        message.schema_location().string(), message.keyword(), message.message());
    for (const auto& detail : message.details()) {
        append_validation_message(diagnostics, detail);
    }
}

void canonicalize(std::vector<JsonSchemaDiagnostic>& diagnostics) {
    const auto fields = [](const JsonSchemaDiagnostic& diagnostic) {
        return std::tie(diagnostic.instance_location(), diagnostic.schema_uri(),
                        diagnostic.schema_location(), diagnostic.keyword(),
                        diagnostic.message());
    };
    std::ranges::sort(diagnostics, [&](const auto& left, const auto& right) {
        return fields(left) < fields(right);
    });
    const auto unique_end = std::ranges::unique(diagnostics, [&](const auto& left,
                                                                 const auto& right) {
        return fields(left) == fields(right);
    });
    diagnostics.erase(unique_end.begin(), diagnostics.end());
}

[[nodiscard]] jsonschema::evaluation_options draft202012_options() {
    return jsonschema::evaluation_options{}.default_version(
        jsonschema::schema_version::draft202012());
}

}  // namespace

class SchemaRegistry final {
public:
    [[nodiscard]] static const SchemaRegistry& instance() {
        static const SchemaRegistry registry;
        return registry;
    }

    [[nodiscard]] JsonSchemaValidationResult validate(const JsonDocument& document,
                                                      JsonSchemaRole role) const {
        const CompiledSchema* compiled = nullptr;
        switch (role) {
            case JsonSchemaRole::content_pack_manifest:
                compiled = &*manifest_schema_;
                break;
            case JsonSchemaRole::resource_definition:
                compiled = &*resource_schema_;
                break;
            default:
                return JsonSchemaValidationResult(
                    {configuration_diagnostic("Unknown JSON schema role")});
        }

        std::vector<JsonSchemaDiagnostic> diagnostics;
        const auto reporter = [&](const jsonschema::validation_message& message)
            -> jsonschema::walk_state {
            append_validation_message(diagnostics, message);
            return jsonschema::walk_state::advance;
        };
        compiled->validate(document.implementation_->value, reporter);
        canonicalize(diagnostics);
        if (diagnostics.empty()) {
            return JsonSchemaValidationResult();
        }
        return JsonSchemaValidationResult(std::move(diagnostics));
    }

private:
    SchemaRegistry() {
        const auto types = parse_embedded(as_bytes(embedded::types_schema,
                                                   embedded::types_schema_size),
                                          "types.schema.json");
        const auto manifest = parse_embedded(
            as_bytes(embedded::content_pack_manifest_schema,
                     embedded::content_pack_manifest_schema_size),
            "content-pack-manifest.schema.json");
        const auto resource = parse_embedded(
            as_bytes(embedded::resource_definition_schema,
                     embedded::resource_definition_schema_size),
            "resource-definition.schema.json");

        meta_validate(types, "types.schema.json");
        meta_validate(manifest, "content-pack-manifest.schema.json");
        meta_validate(resource, "resource-definition.schema.json");

        const auto resolver = [&](const jsoncons::uri& requested) -> jsoncons::json {
            const auto uri = requested.base().string();
            if (uri == types_schema_uri) {
                return types;
            }
            if (uri == manifest_schema_uri) {
                return manifest;
            }
            if (uri == resource_schema_uri) {
                return resource;
            }
            return jsoncons::json::null();
        };

        types_schema_.emplace(
            jsonschema::make_json_schema(types, resolver, draft202012_options()));
        manifest_schema_.emplace(
            jsonschema::make_json_schema(manifest, resolver, draft202012_options()));
        resource_schema_.emplace(
            jsonschema::make_json_schema(resource, resolver, draft202012_options()));
    }

    [[nodiscard]] static jsoncons::json parse_embedded(std::span<const std::byte> bytes,
                                                       std::string_view name) {
        auto parsed = read_json_document(bytes, JsonReaderOptions{bytes.size()});
        if (!parsed.succeeded()) {
            throw std::runtime_error("Embedded schema is not strict JSON: " +
                                     std::string(name));
        }
        return parsed.document()->implementation_->value;
    }

    static void meta_validate(const jsoncons::json& schema, std::string_view name) {
        static const CompiledSchema meta_schema = jsonschema::make_json_schema(
            jsonschema::draft202012::schema_draft202012<jsoncons::json>::get_schema(),
            draft202012_options());

        std::vector<std::string> errors;
        const auto reporter = [&](const jsonschema::validation_message& message)
            -> jsonschema::walk_state {
            errors.push_back(message.instance_location().string() + ": " +
                             message.message());
            return jsonschema::walk_state::advance;
        };
        meta_schema.validate(schema, reporter);
        if (!errors.empty()) {
            throw jsonschema::schema_error("Draft 2020-12 meta-validation failed for " +
                                           std::string(name) + ": " + errors.front());
        }
    }

    std::optional<CompiledSchema> types_schema_;
    std::optional<CompiledSchema> manifest_schema_;
    std::optional<CompiledSchema> resource_schema_;
};

JsonSchemaDiagnostic::JsonSchemaDiagnostic(
    JsonSchemaDiagnosticCode code, std::string instance_location, std::string schema_uri,
    std::string schema_location, std::string keyword, std::string message)
    : code_(code),
      instance_location_(std::move(instance_location)),
      schema_uri_(std::move(schema_uri)),
      schema_location_(std::move(schema_location)),
      keyword_(std::move(keyword)),
      message_(std::move(message)) {}

JsonSchemaDiagnosticCode JsonSchemaDiagnostic::code() const noexcept {
    return code_;
}

const std::string& JsonSchemaDiagnostic::instance_location() const noexcept {
    return instance_location_;
}

const std::string& JsonSchemaDiagnostic::schema_uri() const noexcept {
    return schema_uri_;
}

const std::string& JsonSchemaDiagnostic::schema_location() const noexcept {
    return schema_location_;
}

const std::string& JsonSchemaDiagnostic::keyword() const noexcept {
    return keyword_;
}

const std::string& JsonSchemaDiagnostic::message() const noexcept {
    return message_;
}

JsonSchemaValidationResult::JsonSchemaValidationResult() : result_(std::monostate{}) {}

JsonSchemaValidationResult::JsonSchemaValidationResult(
    std::vector<JsonSchemaDiagnostic> diagnostics)
    : result_(std::move(diagnostics)) {
    assert(!std::get<std::vector<JsonSchemaDiagnostic>>(result_).empty());
}

bool JsonSchemaValidationResult::succeeded() const noexcept {
    return std::holds_alternative<std::monostate>(result_);
}

const std::vector<JsonSchemaDiagnostic>* JsonSchemaValidationResult::diagnostics() const noexcept {
    return std::get_if<std::vector<JsonSchemaDiagnostic>>(&result_);
}

JsonSchemaValidationResult validate_json_document(const JsonDocument& document,
                                                  JsonSchemaRole role) {
    try {
        return SchemaRegistry::instance().validate(document, role);
    } catch (const jsonschema::schema_error& error) {
        return JsonSchemaValidationResult({configuration_diagnostic(error.what())});
    } catch (const std::runtime_error& error) {
        return JsonSchemaValidationResult({configuration_diagnostic(error.what())});
    }
}

}  // namespace corerpg::content_json
