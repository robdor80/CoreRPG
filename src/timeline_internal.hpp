#pragma once
#include <corerpg/timeline.hpp>
#include <array>
#include <type_traits>
namespace corerpg::detail {
// Canonical stable wire tags, deliberately independent of std::variant indices.
inline std::array<std::uint64_t,4> timed_operation(const ActionOperation& operation) noexcept {
    return std::visit([](const auto& op) -> std::array<std::uint64_t,4> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T,PlaceEntityAction>) return {1,op.entity.value(),op.container.value(),0};
        else if constexpr (std::is_same_v<T,RemoveContainmentAction>) return {2,op.entity.value(),0,0};
        else if constexpr (std::is_same_v<T,AddRelationAction>) return {3,op.relation.source.value(),op.relation.kind.value(),op.relation.target.value()};
        else if constexpr (std::is_same_v<T,RemoveRelationAction>) return {4,op.relation.source.value(),op.relation.kind.value(),op.relation.target.value()};
        else return {5,op.stream_id,op.minimum,op.maximum};
    },operation);
}
inline bool timed_operation_valid(const ActionOperation& op, std::uint64_t last) noexcept {
    const auto v=timed_operation(op);
    const auto entity=[&](auto id) { return id>0 && id<=last; };
    switch(v[0]) {
    case 1: return entity(v[1]) && entity(v[2]);
    case 2: return entity(v[1]);
    case 3: case 4: return entity(v[1]) && v[2]>0 && entity(v[3]);
    case 5: return v[2]<=v[3];
    default: return false;
    }
}
inline bool timed_equal(const TimedAction& a,const TimedAction& b) noexcept {
    return a.id==b.id && a.due==b.due && a.parent==b.parent && a.actor==b.actor &&
        timed_operation(a.operation)==timed_operation(b.operation);
}
} // namespace corerpg::detail
