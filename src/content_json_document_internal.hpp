#pragma once

#include <utility>

#include <jsoncons/json.hpp>

#include "corerpg/content_json/json_document_reader.hpp"

namespace corerpg::content_json {

struct JsonDocument::Impl final {
    explicit Impl(jsoncons::json parsed_value) : value(std::move(parsed_value)) {}

    jsoncons::json value;
};

class JsonDocumentAccess final {
public:
    [[nodiscard]] static const jsoncons::json& value(const JsonDocument& document) noexcept {
        return document.implementation_->value;
    }
};

}  // namespace corerpg::content_json
