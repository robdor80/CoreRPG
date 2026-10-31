# History validation — 2026-10-31

Base: `c147d81ed069da2e09149613ab02cbc3ebe95a4a`.
Branch: `codex/core-history-segments-authoritative`.
Consumer authorization: Videojuego_StarTrek PR #76, comment 6536894218.

Local environment: Windows x64, MSVC 19.51.36256, Visual Studio 18 2026,
CMake, Release. Optional Content JSON enabled with existing exact dependency
pins. Original Core checkout and stable consumer pins remain untouched.

Commands (dependency source overrides may point to existing verified caches):

```powershell
cmake -S C:/F04/CoreHistory -B C:/F04/HistoryBuild/core -G "Visual Studio 18 2026" -A x64 -DCORERPG_BUILD_CONTENT_JSON=ON
cmake --build C:/F04/HistoryBuild/core --config Release --parallel 4
ctest --test-dir C:/F04/HistoryBuild/core -C Release --output-on-failure -j 4 --output-junit C:/F04/HistoryBuild/core-tests.xml
```

481 registered tests: **477 passed, 4 existing platform-dependent skips, 0 failed**.
The additions are 16 history/hash tests and 14 process-boundary tests. The latter
write a distinct directory per invocation, terminate without normal C++ cleanup,
then validate from another executable process. They cover no failure and all six
publication boundaries. Existing Core allocation-failure tests still pass.

New negative cases include read injection, missing/truncated/modified segment,
unknown manifest version with recomputed valid SHA-256, modified chain with
valid root hash, forged count with valid root hash, stale writer, duplicated
selection, historical source mutation, identity rollback, pending historical
actor, live containment pin, existing migration destination, and page limits.
SHA-256 known answers include empty input, `abc`, and one million `a` bytes.

The POSIX durability branch is implemented but not executed in this Windows run.
Power-loss behavior beyond the OS/filesystem durability contract is not certified
by software fault injection. The consumer PR must separately demonstrate real
long campaigns, deterministic scheduling, old-save compatibility, and UE NullRHI.
These Core results do not certify F04-PERSISTENCIA-03 complete.

No ABI-breaking layout or signature change was required. The new explicit active
projection contract is documented in HISTORY_V1.md; existing monolithic API
semantics are preserved. Both PRs must remain Draft and must not be merged here.
