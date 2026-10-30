#include <corerpg/timeline.hpp>
#include "timeline_internal.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <tuple>

namespace corerpg {
TimelineStatus Timeline::schedule(const WorldState& world,const TimedAction& action) {
    if (action.id>0 && action.id<=state_.records.size())
        return detail::timed_equal(state_.records[static_cast<std::size_t>(action.id-1)].action,action)
            ? TimelineStatus::no_change : TimelineStatus::key_conflict;
    if (action.id!=state_.records.size()+1 || action.due<state_.now ||
        !world.contains(action.actor) || !detail::timed_operation_valid(action.operation,world.entity_count()) ||
        action.parent>=action.id || (action.parent &&
            state_.records[static_cast<std::size_t>(action.parent-1)].action.due>action.due))
        return TimelineStatus::invalid_request;
    if (state_.records.size()>=max_records) return TimelineStatus::limit_exceeded;
    state_.records.push_back(TimedRecord{action});
    return TimelineStatus::completed;
}
TimelineResult Timeline::advance(WorldState& world,RandomStreams& random,std::uint64_t horizon,
                                  std::size_t limit) noexcept {
    if (horizon<state_.now) return {TimelineStatus::backwards_time,0};
    std::size_t processed=0;
    for (;;) {
        TimedRecord* next=nullptr;
        for (auto& record:state_.records)
            if (record.state==TimedState::pending && record.action.due<=horizon &&
                (!next || std::tie(record.action.due,record.action.id)<std::tie(next->action.due,next->action.id)))
                next=&record;
        if (!next) { state_.now=horizon; return {TimelineStatus::completed,processed}; }
        if (processed==limit) return {TimelineStatus::action_limit,processed};
        auto& record=*next;
        const auto parent=record.action.parent;
        if (parent && state_.records[static_cast<std::size_t>(parent-1)].state!=TimedState::accepted &&
            state_.records[static_cast<std::size_t>(parent-1)].state!=TimedState::no_change) {
            record.state=TimedState::causal_rejected;
            record.revision_before=record.revision_after=world.revision();
        } else {
            try {
                const auto result=ActionEngine::process(world,random,{record.action.actor,record.action.operation});
                record.state=result.outcome==ActionOutcome::accepted ? TimedState::accepted :
                    result.outcome==ActionOutcome::no_change ? TimedState::no_change : TimedState::rejected;
                record.revision_before=result.revision_before;
                record.revision_after=result.revision_after;
                record.rejection=result.rejection;
                if (const auto* roll=std::get_if<RollConsequence>(&result.consequence)) {
                    record.has_roll=true; record.roll=roll->value;
                }
            } catch (const std::bad_alloc&) { return {TimelineStatus::allocation_failure,processed}; }
              catch (const std::length_error&) { return {TimelineStatus::capacity_failure,processed}; }
        }
        state_.now=record.action.due;
        ++processed;
    }
}
bool Timeline::validate(const TimelineSnapshot& snapshot,std::uint64_t last,std::uint64_t revision) noexcept {
    if (snapshot.records.size()>max_records) return false;
    const TimedRecord* last_completed=nullptr;
    for (std::size_t i=0;i<snapshot.records.size();++i) {
        const auto& r=snapshot.records[i]; const auto& a=r.action;
        if (a.id!=i+1 || !a.actor.is_valid() || a.actor.value()>last ||
            !detail::timed_operation_valid(a.operation,last) || a.parent>=a.id ||
            (a.parent && snapshot.records[static_cast<std::size_t>(a.parent-1)].action.due>a.due) ||
            static_cast<unsigned>(r.state)>static_cast<unsigned>(TimedState::causal_rejected)) return false;
        if (r.state==TimedState::pending) {
            if (a.due<snapshot.now || r.revision_before || r.revision_after || r.roll || r.has_roll ||
                r.rejection!=ActionRejection::none) return false;
            continue;
        }
        if (a.due>snapshot.now || r.revision_before>r.revision_after || r.revision_after>revision ||
            static_cast<unsigned>(r.rejection)>static_cast<unsigned>(ActionRejection::invalid_operation)) return false;
        if (r.state==TimedState::rejected) {
            if (r.rejection==ActionRejection::none || r.revision_before!=r.revision_after) return false;
        } else if (r.rejection!=ActionRejection::none) return false;
        if ((r.state==TimedState::no_change || r.state==TimedState::causal_rejected) &&
            r.revision_before!=r.revision_after) return false;
        if (!r.has_roll && r.roll) return false;
        const auto op=detail::timed_operation(a.operation);
        if ((r.state==TimedState::accepted && op[0]==5)!=r.has_roll) return false;
        if (r.has_roll && (op[0]!=5 || r.state!=TimedState::accepted || r.roll<op[2] || r.roll>op[3])) return false;
        if (r.state==TimedState::accepted && (op[0]==5 ? r.revision_before!=r.revision_after :
            r.revision_after-r.revision_before!=1)) return false;
        if (a.parent) {
            const auto p=snapshot.records[static_cast<std::size_t>(a.parent-1)].state;
            if (p==TimedState::pending || ((p==TimedState::accepted || p==TimedState::no_change) ==
                (r.state==TimedState::causal_rejected))) return false;
        } else if (r.state==TimedState::causal_rejected) return false;
        if (!last_completed || std::tie(last_completed->action.due,last_completed->action.id)<std::tie(a.due,a.id)) last_completed=&r;
    }
    if (last_completed) for (const auto& r:snapshot.records)
        if (r.state==TimedState::pending && std::tie(r.action.due,r.action.id)<
            std::tie(last_completed->action.due,last_completed->action.id)) return false;
    // Revisions follow execution order even when scheduling order differs.
    for (const auto& a:snapshot.records) if (a.state!=TimedState::pending)
        for (const auto& b:snapshot.records) if (b.state!=TimedState::pending &&
            std::tie(a.action.due,a.action.id)<std::tie(b.action.due,b.action.id) &&
            a.revision_after>b.revision_before) return false;
    return true;
}
} // namespace corerpg
