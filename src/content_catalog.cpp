#include "corerpg/content_catalog.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace corerpg {
namespace {

struct DependencyTask final {
    PackId source;
    PackId target;
    VersionRequirement requirement;
};

struct ReferenceTask final {
    DefinitionId source;
    DefinitionReference reference;
};

[[nodiscard]] bool version_requirement_less(const VersionRequirement& left,
                                            const VersionRequirement& right) noexcept {
    if (left.minimum_inclusive() != right.minimum_inclusive()) {
        return left.minimum_inclusive() < right.minimum_inclusive();
    }
    return left.maximum_exclusive() < right.maximum_exclusive();
}

[[nodiscard]] bool dependency_task_less(const DependencyTask& left,
                                        const DependencyTask& right) noexcept {
    if (left.source != right.source) {
        return left.source < right.source;
    }
    if (left.target != right.target) {
        return left.target < right.target;
    }
    return version_requirement_less(left.requirement, right.requirement);
}

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
    if (left.pack_id() != right.pack_id()) {
        return left.pack_id() < right.pack_id();
    }
    if (left.definition_id() != right.definition_id()) {
        return left.definition_id() < right.definition_id();
    }
    if (left.related_pack_id() != right.related_pack_id()) {
        return left.related_pack_id() < right.related_pack_id();
    }
    if (left.related_definition_id() != right.related_definition_id()) {
        return left.related_definition_id() < right.related_definition_id();
    }
    if (left.code() != right.code()) {
        return left.code() < right.code();
    }
    if (left.version_requirement().has_value() != right.version_requirement().has_value()) {
        return !left.version_requirement().has_value();
    }
    if (left.version_requirement().has_value() &&
        version_requirement_less(*left.version_requirement(), *right.version_requirement())) {
        return true;
    }
    if (left.version_requirement().has_value() &&
        version_requirement_less(*right.version_requirement(), *left.version_requirement())) {
        return false;
    }
    if (left.actual_version() != right.actual_version()) {
        return left.actual_version() < right.actual_version();
    }
    if (left.expected_kind() != right.expected_kind()) {
        return left.expected_kind() < right.expected_kind();
    }
    return left.actual_kind() < right.actual_kind();
}

void sort_diagnostics(std::vector<ContentDiagnostic>& diagnostics) {
    std::ranges::sort(diagnostics, diagnostic_less);
}

[[nodiscard]] auto find_pack(std::vector<ContentPackInput>& packs, const PackId& pack_id) {
    return std::ranges::lower_bound(
        packs, pack_id, {},
        [](const ContentPackInput& input) -> const PackId& { return input.manifest().pack_id(); });
}

[[nodiscard]] bool has_direct_dependency(const std::vector<DependencyTask>& dependencies,
                                         const PackId& source,
                                         const PackId& target) noexcept {
    return std::ranges::any_of(dependencies, [&](const DependencyTask& dependency) {
        return dependency.source == source && dependency.target == target;
    });
}

}  // namespace

ContentCatalog::ContentCatalog(std::vector<ResourceDefinition> definitions)
    : definitions_(std::move(definitions)) {}

CatalogBuildResult ContentCatalog::create(std::vector<ContentPackInput> packs) {
    std::ranges::sort(packs, {}, [](const ContentPackInput& input) -> const PackId& {
        return input.manifest().pack_id();
    });

    std::vector<ContentDiagnostic> diagnostics;

    for (std::size_t index = 0; index < packs.size();) {
        std::size_t next = index + 1;
        while (next < packs.size() &&
               packs[next].manifest().pack_id() == packs[index].manifest().pack_id()) {
            ++next;
        }
        if (next - index > 1) {
            diagnostics.push_back(
                ContentDiagnostic::duplicate_pack(packs[index].manifest().pack_id()));
        }
        index = next;
    }

    if (!diagnostics.empty()) {
        return CatalogBuildResult(std::move(diagnostics));
    }

    for (const auto& pack : packs) {
        for (const auto& definition : pack.definitions()) {
            if (definition.definition().id().pack_id() != pack.manifest().pack_id()) {
                diagnostics.push_back(ContentDiagnostic::definition_pack_mismatch(
                    pack.manifest().pack_id(), definition.definition().id()));
            }
        }
    }

    if (!diagnostics.empty()) {
        sort_diagnostics(diagnostics);
        return CatalogBuildResult(std::move(diagnostics));
    }

    std::vector<DependencyTask> dependencies;
    for (const auto& pack : packs) {
        dependencies.reserve(dependencies.size() + pack.manifest().dependencies().size());
        for (const auto& dependency : pack.manifest().dependencies()) {
            dependencies.push_back(DependencyTask{pack.manifest().pack_id(),
                                                  dependency.pack_id(),
                                                  dependency.requirement()});
        }
    }
    std::ranges::sort(dependencies, dependency_task_less);

    for (std::size_t index = 0; index < dependencies.size();) {
        std::size_t next = index + 1;
        while (next < dependencies.size() &&
               dependencies[next].source == dependencies[index].source &&
               dependencies[next].target == dependencies[index].target) {
            ++next;
        }
        if (next - index > 1) {
            diagnostics.push_back(ContentDiagnostic::duplicate_dependency(
                dependencies[index].source, dependencies[index].target));
        }
        if (dependencies[index].source == dependencies[index].target) {
            diagnostics.push_back(
                ContentDiagnostic::self_dependency(dependencies[index].source));
        }
        index = next;
    }

    if (!diagnostics.empty()) {
        sort_diagnostics(diagnostics);
        return CatalogBuildResult(std::move(diagnostics));
    }

    for (const auto& dependency : dependencies) {
        const auto target = find_pack(packs, dependency.target);
        if (target == packs.end() || target->manifest().pack_id() != dependency.target) {
            diagnostics.push_back(ContentDiagnostic::dependency_not_found(
                dependency.source, dependency.target));
            continue;
        }
        if (!dependency.requirement.is_satisfied_by(target->manifest().pack_version())) {
            diagnostics.push_back(ContentDiagnostic::dependency_version_mismatch(
                dependency.source, dependency.target, dependency.requirement,
                target->manifest().pack_version()));
        }
    }

    if (!diagnostics.empty()) {
        sort_diagnostics(diagnostics);
        return CatalogBuildResult(std::move(diagnostics));
    }

    std::vector<std::vector<std::size_t>> adjacency(packs.size());
    for (const auto& dependency : dependencies) {
        const auto source = find_pack(packs, dependency.source);
        const auto target = find_pack(packs, dependency.target);
        const auto source_index = static_cast<std::size_t>(source - packs.begin());
        const auto target_index = static_cast<std::size_t>(target - packs.begin());
        adjacency[source_index].push_back(target_index);
    }

    enum class VisitState { unvisited, visiting, visited };
    std::vector<VisitState> states(packs.size(), VisitState::unvisited);
    const auto visit = [&](auto&& self, std::size_t source_index) -> void {
        states[source_index] = VisitState::visiting;
        for (const auto target_index : adjacency[source_index]) {
            if (states[target_index] == VisitState::unvisited) {
                self(self, target_index);
            } else if (states[target_index] == VisitState::visiting) {
                diagnostics.push_back(ContentDiagnostic::dependency_cycle(
                    packs[source_index].manifest().pack_id(),
                    packs[target_index].manifest().pack_id()));
            }
        }
        states[source_index] = VisitState::visited;
    };

    for (std::size_t index = 0; index < packs.size(); ++index) {
        if (states[index] == VisitState::unvisited) {
            visit(visit, index);
        }
    }

    if (!diagnostics.empty()) {
        sort_diagnostics(diagnostics);
        return CatalogBuildResult(std::move(diagnostics));
    }

    std::vector<ContentDefinitionInput> definitions;
    for (const auto& pack : packs) {
        definitions.reserve(definitions.size() + pack.definitions().size());
        for (const auto& definition : pack.definitions()) {
            definitions.push_back(definition);
        }
    }
    std::ranges::sort(definitions, {}, [](const ContentDefinitionInput& input) -> const auto& {
        return input.definition().id();
    });

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
        sort_diagnostics(diagnostics);
        return CatalogBuildResult(std::move(diagnostics));
    }

    for (const auto& task : references) {
        const auto& source_pack = task.source.pack_id();
        const auto& target_pack = task.reference.target().pack_id();
        if (source_pack != target_pack &&
            !has_direct_dependency(dependencies, source_pack, target_pack)) {
            diagnostics.push_back(ContentDiagnostic::cross_pack_reference_not_declared(
                task.source, task.reference.target()));
        }
    }

    if (!diagnostics.empty()) {
        sort_diagnostics(diagnostics);
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
