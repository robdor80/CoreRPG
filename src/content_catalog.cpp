#include "corerpg/content_catalog.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace corerpg {
namespace {

struct ReferenceTask final {
    DefinitionId source;
    DefinitionReference reference;
};

[[nodiscard]] bool reference_task_less(const ReferenceTask& left,
                                       const ReferenceTask& right) noexcept {
    if (left.source != right.source) {
        return left.source < right.source;
    }
    if (left.reference.target() != right.reference.target()) {
        return left.reference.target() < right.reference.target();
    }
    return left.reference.expected_kind() < right.reference.expected_kind();
}

[[nodiscard]] bool same_reference_task(const ReferenceTask& left,
                                       const ReferenceTask& right) noexcept {
    return left.source == right.source &&
           left.reference.target() == right.reference.target() &&
           left.reference.expected_kind() == right.reference.expected_kind();
}

[[nodiscard]] bool diagnostic_less(const ContentDiagnostic& left,
                                   const ContentDiagnostic& right) noexcept {
    if (left.definition_id() != right.definition_id()) {
        return left.definition_id() < right.definition_id();
    }
    if (left.related_definition_id() != right.related_definition_id()) {
        return left.related_definition_id() < right.related_definition_id();
    }
    if (left.code() != right.code()) {
        return left.code() < right.code();
    }
    if (left.expected_kind() != right.expected_kind()) {
        return left.expected_kind() < right.expected_kind();
    }
    return left.actual_kind() < right.actual_kind();
}

}  // namespace

ContentCatalog::ContentCatalog(std::vector<ResourceDefinition> definitions)
    : definitions_(std::move(definitions)) {}

CatalogBuildResult ContentCatalog::create(std::vector<ContentDefinitionInput> definitions) {
    std::ranges::sort(definitions, {}, [](const ContentDefinitionInput& input) -> const auto& {
        return input.definition().id();
    });

    std::vector<ContentDiagnostic> diagnostics;
    for (std::size_t index = 0; index < definitions.size();) {
        std::size_t next = index + 1;
        while (next < definitions.size() &&
               definitions[next].definition().id() == definitions[index].definition().id()) {
            ++next;
        }
        if (next - index > 1) {
            diagnostics.push_back(ContentDiagnostic::duplicate_definition(
                definitions[index].definition().id()));
        }
        index = next;
    }

    if (!diagnostics.empty()) {
        return CatalogBuildResult(std::move(diagnostics));
    }

    std::vector<ResourceDefinition> staged_definitions;
    std::vector<ReferenceTask> references;
    staged_definitions.reserve(definitions.size());

    for (const auto& input : definitions) {
        staged_definitions.push_back(input.definition());
        references.reserve(references.size() + input.references().size());
        for (const auto& reference : input.references()) {
            references.push_back(ReferenceTask{input.definition().id(), reference});
        }
    }

    std::ranges::sort(references, reference_task_less);
    const auto unique_end = std::ranges::unique(references, same_reference_task).begin();
    references.erase(unique_end, references.end());

    for (const auto& task : references) {
        const auto target = std::ranges::lower_bound(
            staged_definitions, task.reference.target(), {}, &ResourceDefinition::id);
        if (target == staged_definitions.end() || target->id() != task.reference.target()) {
            diagnostics.push_back(ContentDiagnostic::reference_target_not_found(
                task.source, task.reference.target(), task.reference.expected_kind()));
            continue;
        }

        if (target->kind() != task.reference.expected_kind()) {
            diagnostics.push_back(ContentDiagnostic::reference_kind_mismatch(
                task.source, task.reference.target(), task.reference.expected_kind(),
                target->kind()));
        }
    }

    if (!diagnostics.empty()) {
        std::ranges::sort(diagnostics, diagnostic_less);
        return CatalogBuildResult(std::move(diagnostics));
    }

    return CatalogBuildResult(ContentCatalog(std::move(staged_definitions)));
}

std::size_t ContentCatalog::size() const noexcept {
    return definitions_.size();
}

bool ContentCatalog::empty() const noexcept {
    return definitions_.empty();
}

bool ContentCatalog::contains(const DefinitionId& id) const noexcept {
    return find(id) != nullptr;
}

const ResourceDefinition* ContentCatalog::find(const DefinitionId& id) const noexcept {
    const auto definition = std::ranges::lower_bound(definitions_, id, {}, &ResourceDefinition::id);
    if (definition == definitions_.end() || definition->id() != id) {
        return nullptr;
    }

    return &*definition;
}

const std::vector<ResourceDefinition>& ContentCatalog::definitions() const noexcept {
    return definitions_;
}

CatalogBuildResult::CatalogBuildResult(ContentCatalog catalog)
    : result_(std::move(catalog)) {}

CatalogBuildResult::CatalogBuildResult(std::vector<ContentDiagnostic> diagnostics)
    : result_(std::move(diagnostics)) {
    assert(!std::get<std::vector<ContentDiagnostic>>(result_).empty());
}

bool CatalogBuildResult::succeeded() const noexcept {
    return std::holds_alternative<ContentCatalog>(result_);
}

const ContentCatalog* CatalogBuildResult::catalog() const noexcept {
    return std::get_if<ContentCatalog>(&result_);
}

const std::vector<ContentDiagnostic>* CatalogBuildResult::diagnostics() const noexcept {
    return std::get_if<std::vector<ContentDiagnostic>>(&result_);
}

}  // namespace corerpg
