#include "authorized_context_host.hpp"

#include <array>

int main() {
    using namespace corerpg;
    WorldState world;
    RandomStreams random{42};
    const auto actor = world.create_entity();
    const auto destination = world.create_entity();
    if (world.learn_fact(actor, {actor}) != KnowledgeChange::applied ||
        world.learn_fact(actor, {destination}) != KnowledgeChange::applied) return 1;
    const auto view = Visibility::project(world, actor);
    std::array<VisibleFact, 8> storage;
    const auto context = ContextBuilder::build(view, {}, storage);
    if (context.status != ContextStatus::complete || context.emitted != 2) return 2;
    // Host adapter serializes version/status and std::span{storage}.first(context.emitted),
    // filter classification and apply its own byte/token limit. No remote call.
    const example_host::AuthenticatedSession session{actor, 1, true};
    const example_host::IssuedContext issued{7, session.epoch, actor, world.revision()};
    const example_host::PlacementProposal proposal{issued.id, actor, destination};
    const auto result = example_host::evaluate(world, random, session, issued, view, proposal);
    if (!result.technical || result.technical->outcome != ActionOutcome::accepted) return 3;
    // UI/provider receives only a fresh authorized context, never result.technical.
    return 0;
}
