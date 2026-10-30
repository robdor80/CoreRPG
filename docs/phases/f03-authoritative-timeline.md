# F03: additive authoritative campaign time

Base: `4f282863737bcd8f5add401202a9e1064c133672`. The audit found existing identity,
structural population, independent random streams, ActionEngine, Simulation,
Visibility, Knowledge, ContextBuilder and SAVE v1-v3. `Simulation::step` explicitly
does not own a clock or queue. These changes add that missing generic capability
without Star Trek content or Unreal dependencies.

`Timeline::schedule` accepts owned `TimedAction` values. IDs are contiguous
idempotency keys starting at 1; exact replay is a no-op and conflicting reuse is
rejected. At most 4096 records are retained (including terminal records). The
clock is an unsigned integer tick count; content supplies epoch and tick duration.
`advance` processes `(due, id)` order via the existing ActionEngine, even without
rendering. A parent must have a lower ID and no later due time. Rejected parents
cause terminal `causal_rejected` children without executing their operations.
Successful/no-op parents permit execution. No implicit retries or wall-clock/FPS
advancement occur.

Atomicity is per action. On budget exhaustion or allocation failure, the completed
prefix remains committed and the failing action stays pending; resume the same
horizon. The clock does not pass pending work. A same-horizon replay after success
executes zero actions. Scheduling and advancing require an authenticated privileged
host. Actor identity is not authorization. This minimal queue supports the five
existing ActionOperation variants; it does not borrow RuleCheck spans or retain
expected revisions. The engine rechecks structural validity at execution. Domain
authorization must be settled before enqueueing; there is no persisted revocable
permission or general job lifecycle in this increment.

`CampaignPersistence` owns an additive archive with both an unchanged SAVE v1-v3
payload and the timeline. Restore validates both, allocates the replacement agenda,
then calls the existing atomic world/RNG restore and swaps the agenda without
throwing. Every destination survives a decode, semantic, catalog or allocation
failure. Old Persistence APIs and golden wire tests are unchanged. They reject the
new magic rather than silently losing the clock. No automatic legacy migration is
claimed. Applications with timelines must use CampaignPersistence for complete saves.

## Archive format 1

Little endian. Magic `CRPGTIME` (8), version `1` (u32), embedded Core save size (u64),
current tick (u64), record count (u64), tick policy `1` (u64: opaque integer ticks).
Header is 44 bytes. Then the complete embedded Core save including its own CRC32C.
Each record is fourteen u64 words (112 bytes): id, due, parent, actor, operation tag,
a, b, c, state, revision_before, revision_after, roll, rejection, has_roll.
Operation tags 1..5 are place, remove containment, add relation, remove relation,
stream roll. Arguments match those existing primitives. Unused words must be zero.
State tags 0..4 are pending, accepted, no_change, rejected, causal_rejected.
Finally CRC32C over all preceding archive bytes (u32).

Decoder bounds counts and exact length before allocating, verifies both checksums,
rejects future versions/policies, unknown tags, invalid booleans, missing entities,
noncontiguous keys, impossible times, parents, outcomes and revision ordering.
CRC is accidental-corruption detection, not authenticity against malicious editing.
The archive is limited by PersistenceLimits.max_bytes and Timeline.max_records.
No filesystem/durability policy is imposed by Core.

## Reproduction and scope

```powershell
cmake -S . -B C:/F03/core-build -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON -DCORERPG_BUILD_CONTENT_JSON=ON
cmake --build C:/F03/core-build --config Release --parallel 4
ctest --test-dir C:/F03/core-build -C Release --no-tests=error --output-on-failure --parallel 4
```

Tests cover order, same-tick ties, causal rejection, offscreen changes, bounded
resume, duplicate suppression, all five operations, knowledge, future RNG draws,
roundtrip into new objects, every byte corruption/truncation, incompatible versions,
semantic rejection, maximum tick and record capacity. An isolated executable injects
allocation failure into schedule, advance and every allocation of archive load.
This is a minimal vertical: no galactic simulator, travel model, inferred narrative,
memory aging, arbitrary domain properties, asynchronous commands, cancellation,
multithreading or event-store compaction. History is privileged; project it through
domain authorization before exposing it to an observer.
