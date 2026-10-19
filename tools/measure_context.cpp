// Developer measurement only. No timing assertion and no dependency on a provider.
// c++ -std=c++20 -O3 -Iinclude tools/measure_context.cpp build/core-only/libCoreRPG.a -o /tmp/measure-context
#include <corerpg/context_builder.hpp>
#include <corerpg/world_state.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <vector>

int main() {
    using namespace corerpg;
    using Clock = std::chrono::steady_clock;
    std::uint64_t checksum = 0;
    std::cout << "K,projection_median_us,prefix_selection_median_us,directed_selection_median_us,visible_record_bytes\n";
    for (const std::size_t count : {128U, 4096U, 65536U, 65538U}) {
        WorldState world;
        const auto actor = world.create_entity();
        for (std::size_t i = 0; i < count; ++i) {
            const auto id = world.create_entity();
            if (world.learn_fact(actor, {id}) != KnowledgeChange::applied) return 1;
        }
        std::vector<double> projection, prefix, filtered;
        std::array<VisibleFact, 128> output;
        for (int i = 0; i < 51; ++i) {
            auto start = Clock::now();
            const auto view = Visibility::project(world, actor);
            auto end = Clock::now();
            projection.push_back(std::chrono::duration<double, std::micro>(end - start).count());
            start = Clock::now();
            const auto first = ContextBuilder::build(view, {}, output);
            end = Clock::now();
            prefix.push_back(std::chrono::duration<double, std::micro>(end - start).count());
            start = Clock::now();
            const auto last = ContextBuilder::build(view, {1, EntityId{count + 1}}, output);
            end = Clock::now();
            filtered.push_back(std::chrono::duration<double, std::micro>(end - start).count());
            checksum += first.examined + last.examined + first.emitted + last.emitted;
            if (first.emitted != 128 || first.examined > 129 || last.examined != 1 || last.emitted != 1 || last.status != ContextStatus::complete) return 2;
        }
        for (auto* series : {&projection, &prefix, &filtered}) std::sort(series->begin(), series->end());
        std::cout << count << ',' << projection[25] << ',' << prefix[25] << ',' << filtered[25]
                  << ',' << count * sizeof(VisibleFact) << '\n';
    }
    std::cout << "checksum=" << checksum << ",sizeof(VisibleFact)=" << sizeof(VisibleFact) << '\n';
}
