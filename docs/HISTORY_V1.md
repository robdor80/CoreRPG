# Opt-in authoritative history, storage version 1

This is an additive generic API in `corerpg/history.hpp`. It contains no game
rules. Existing WorldState, Persistence, CampaignPersistence, Timeline, SAVE
v1/v2/v3 and CRPGTIME v1 signatures, layout and wire semantics remain unchanged.
The only edits to existing class declarations are friend declarations. Existing
monolithic campaigns use precisely their previous API and behavior.

## Identity and active projection

Migration explicitly opts a campaign into `HistoryArchive`. The WorldState
passed to this API is its **active projection**, together with RNG and Timeline.
Issued 64-bit identities remain the dense interval `[1, high_water_mark]`.
Consolidation never decrements the watermark, renumbers or reissues an ID.
`WorldState::contains`, `entity_count`, and `next_entity_id` retain their original
issued-ID semantics. They are intentionally not residency queries. The new
`locate`, `resident_count`, `archived_count` and sparse `bound_entities` APIs
provide the explicit operational distinction. An archived record has no payload
in the active projection; its complete original payload is in an immutable
segment. `definition_of` on an active projection reads only that projection.

Consumers must opt in throughout their persistence boundary. Saving the active
projection alone as a full campaign would omit history; a consumer must reject
that operation and route the new mode through HistoryPersistence. No old reader
is silently upgraded to a segment reader. Old readers reject the new magic.

Each mutation is staged by the consumer as before. At publication Core rejects
any definitions, outgoing relations, containment or knowledge reintroduced for
archived sources, decreasing issued IDs/revision/time, invalid references,
duplicate ownership, or invalid Core/Timeline state. Targets of directed
relations may be historical; logical existence is independent of residency.
Historical containers, population groups/members and pending Timeline targets
cannot be retired from operational use. Population retirement is deliberately
unsupported in v1. Closed historical relations do not acquire operational force.
Domain validators decide temporal closure and retain facts needed by their rules.

## Original records and reductions

The caller supplies an ID-sorted set of closed facts with declared `[begin,end)`
intervals and a monotonic consolidation frontier no later than the current clock.
Core partitions the exact existing definitions, outgoing relations, containment
and observer knowledge by source ID. It never synthesizes facts or replays RNG.
All active streams, seed, clock and Timeline remain in the same checkpoint.
Consolidation increases revision once; it consumes no RNG and changes no IDs.

Count rules are declarative, versioned manifest data. A rule specifies a definition,
a grouping relation and a disjunction of scalar predicates. Scalars are explicitly
defined for this optional feature as self-relations: high 16 bits identify a field,
low 48 bits its value. No particular field or definition has a built-in meaning.
Consolidation derives exact integer counts from original selected records in Core.
Recovery recomputes the counts from every original segment and requires equality
with the manifest. Counts are never user-supplied replacements for original facts.
Grouped identities must remain operational; consumers use counts plus current
records where lifetime history affects future decisions. Counts have overflow
checks and no RNG effect. Rule definitions cannot change after migration.

## Storage and publication protocol

All integers are unsigned little-endian. Lengths are checked before record
allocation. Default per-object record limits remain 64 MiB / one million sparse
records. The additional defaults are 16 MiB manifest, 100,000 segments, 65,536
selected records per segment and 256 records per query page. No previous limit
is raised. Total on-disk history grows with actual facts; it is not discarded.

Files are content-addressed `<SHA-256>.obj`:

* `CRPGSEG1`, u32 version 1, 16-byte campaign identity, sequence, previous segment
  SHA-256, bounding interval, exact ID/time index and canonical Core SAVE original
  records. Descriptors include length, issued watermark, revision and compressed
  inclusive ID ranges. Index equality and payload ownership are checked on read.
* `CRPGHST1`, u32 version 1, campaign identity, generation, issued watermark,
  revision, frontier, clock, active checkpoint SHA-256/length, ordered segment
  descriptors, count rules and counts. Noncanonical ranges, overlapping ID
  ownership, unknown versions and altered chains are rejected.
* The checkpoint is unmodified CRPGTIME v1, containing active state only.
* `CURRENT`: `CRPGROOT`, u32 version 1, generation and manifest SHA-256.

Segments can have overlapping bounding time intervals when a dependency pins an
older fact until a later consolidation. Their **ID ownership cannot overlap**;
declared temporal coverage never substitutes for the exact entity index.

A writer obtains an OS process lock in `writer.lock`; an abandoned process does
not leave a stale logical lock. It compares CURRENT with its expected generation
and root. Stale writers fail with conflict. It verifies every existing segment,
stages the new segment, flushes it, reads and checks it, stages/flushes/verifies
the full checkpoint, then stages/flushes/verifies the manifest. Finally it
atomically replaces CURRENT. Windows uses CreateFile/WriteFile,
FlushFileBuffers and MoveFileExW with REPLACE_EXISTING and WRITE_THROUGH. POSIX
uses write/fsync/rename and directory fsync. All return values are checked.
These are local-filesystem guarantees; a filesystem/device must honor those
primitives. Network filesystems are not certified by these tests.

All allocations and restores needed for in-memory publication precede CURRENT.
An optional consumer projection validator runs on the staged checkpoint before
any publication, so domain validation can reject a proposed retirement atomically.
Before publication failures preserve live destinations and the prior root.
An I/O error during pointer publication can have uncertain commit outcome:
**recover before retrying or using the old projection**. The injected
`after_publish` process-loss boundary deliberately leaves old memory and a new
durable root; a stale writer cannot overwrite it. Recovery selects exactly the
root in CURRENT, streams and validates all segments and reductions, checks the
complete checkpoint, and only then replaces all live destinations together.
It never mixes generations or silently falls back to older history after
confirmed corruption. Older objects are retained for explicit recovery/audit.
Unreferenced temporaries/objects are ignored, never treated as committed facts;
there is no automatic deletion or garbage collection in this version.

SHA-256 checks integrity, not authorship. Anyone able to replace the trusted root
and every object can forge a different campaign. Root authentication, encryption,
cloud replication and automatic repair are outside this change.

## Queries, migration and verification

`query(archive, after_id, limit)` returns original full facts in global ID order
with an exclusive cursor, `more`, and explicit error status. It does not activate
records. Keep the same archive generation across pages. Its temporary working
set is one bounded segment plus the result page, range metadata and at most
limit+1 candidate IDs. Missing/truncated/corrupt storage is an error, never an
empty-history answer. Privileged raw facts require consumer authorization just
like Inspector; they are not observer-safe projections by themselves.

Migration is explicit: load the original through existing readers, validate it,
then `migrate` into a **new directory** and supply a nonzero campaign identity.
The original is never opened for writing or removed. Existing destinations are
rejected even when empty. Verify equivalent active bytes, IDs, RNG, time and
future behavior before switching consumers. Old backups remain rollback inputs.

The test suite covers original payload/cross-reference retention, identity
nonreuse, count reconstruction, migration, stale writers, pre/post publication
faults, missing/truncated/corrupt segment reads, bounded queries, and OS process
termination followed by fresh-process recovery at every commit boundary. It also
runs the complete existing Core and optional JSON content suites. Consumer-scale
campaign evidence belongs in the consumer PR and is not implied by Core unit tests.
