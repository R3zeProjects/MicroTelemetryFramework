# Benchmark methodology

`MicroTelemetryFrameworkBenchmark` is an opt-in repository target. It is never
installed or exported by the CMake package.

The native benchmark executes three one-million-operation scenarios:

1. relaxed atomic increments through a copied `Counter` handle;
2. scalar construction and publication of owning event records through an
   async pipeline configured with capacity 1024 and batch size 64;
3. publication of owning records in batches of 64 through the same pipeline.

The async exporter performs one relaxed atomic addition per batch and verifies
that all records were delivered. Timing includes queue contention,
backpressure, record moves, worker wakeups, and exporter invocation. It excludes
process startup and pipeline construction.

The 2026-08-26 Clang 22.1.6 Release verification used seven processes and
measured 116.681M counter updates/s, 1.987M scalar async records/s, and 18.222M
batched async records/s. The retained samples are in
[`current-native-raw-2026-08-26.csv`](../benchmark-results/current-native-raw-2026-08-26.csv).
Compare
versions only with the same compiler, machine, power plan, iteration count, and
benchmark source. Debug numbers and results from different machines must not be
presented as regressions or improvements.

## Batched async publication (2026-08-23)

The span overload now reserves available queue capacity and publishes a chunk
under one lock and notification instead of repeating the scalar path for every
record. Backpressure, bounded capacity, ownership, shutdown, and partial
acceptance semantics are unchanged. Nine alternating A/B runs measured
24.666M records/s for batches of 64 versus 10.418M/s (**+136.76%**); the
observed ranges did not overlap. The scalar async median improved by 23.39%,
but its wider overlapping ranges make that result directional rather than a
hard regression gate. Evidence is stored in
[`v0.1.1-batch-medians-2026-08-23.csv`](../benchmark-results/v0.1.1-batch-medians-2026-08-23.csv).

The fixed-capacity power-of-two ring experiment was rejected: it reduced the
measured async median by 10.3% compared with the existing deque. Bit masking is
therefore not used where it makes the real workload slower.

## External comparison

`MicroTelemetryFrameworkExternalBenchmark` is enabled by
`MTF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS=ON`. CMake fetches immutable commits:

Only the source trees required by the comparison are fetched. Upstream test,
tool, and benchmark submodules are deliberately disabled; they are not linked
into the comparison and can exceed legacy Windows path limits.

- prometheus-cpp 1.3.0: `e5fada43131d251e9c4786b04263ce98b6767ba5`;
- OpenTelemetry C++ 1.9.1: `770fce3c4095f6dc852fd80fb0810936b723be9a`.

Each timed counter loop adds `1.0` to one unlabelled instrument. Each histogram
loop observes `4.2`; MTF and prometheus-cpp use boundaries 1, 5, 25, and 100.
OpenTelemetry uses its SDK default histogram aggregation. Instrument creation
and final collection are excluded. MTF, prometheus-cpp, and OpenTelemetry
collection are validated after timing. The atomic baseline quantifies the
minimum synchronization cost on this machine.

The 2026-08-26 recorded values are seven-run medians from a Clang 22.1.6 Release
build on Windows and AMD Ryzen 7 PRO 1700X:

| Scenario | MTF | prometheus-cpp 1.3.0 | OpenTelemetry C++ 1.9.1 |
| --- | ---: | ---: | ---: |
| Counter updates/s | 124.499M | 135.966M | 25.920M |
| Histogram observations/s | 56.930M | 24.664M | 27.106M |

The counter atomic baseline was 124.521M updates/s. These loops exclude
exporters, protocol serialization, labels, and instrument creation; they do not
claim feature parity with Prometheus exposition or OpenTelemetry signals. Raw
samples are stored in
[`external-telemetry-raw-2026-08-26.csv`](../benchmark-results/external-telemetry-raw-2026-08-26.csv).

```sh
cmake -S . -B build-external -DCMAKE_BUILD_TYPE=Release \
  -DMTF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS=ON
cmake --build build-external --parallel \
  --target MicroTelemetryFrameworkExternalBenchmark
./build-external/MicroTelemetryFrameworkExternalBenchmark 1000000
```

## Full ecosystem benchmark

`MicroTelemetryFrameworkEcosystemBenchmark` is enabled by
`MTF_BUILD_ECOSYSTEM_BENCHMARKS=ON`. It pins:

- MCF `39996b3b2f2f20860e8e396c12165c096b924999`;
- MEF `7692a00b333dfc7549893ec5bb0e91b8db655307`;
- MPF `ab76da8993120331ca7b87755f319550fd5c8267`;
- the checked-out MTF source.

One transaction creates one MEF log and one MTF event, updates one counter, and
stores both owning records in a pre-reserved thread-safe in-memory MPF target.
The timer includes value construction, logger dispatch, conversion, queueing
where selected, and persistence copies. It excludes setup and final validation.

The repaired 2026-08-26 integration target uses current public headers
(`<vosp/error.hpp>`, `<vosp/logger.hpp>`, and `<vosp/persistence.hpp>`) and the
independent MPF `OperationResult`. Seven Release processes measured 4.679M
direct transactions/s and 1.841M bounded-async transactions/s. Every process
validated 200,000 owning persisted records. Raw samples are stored in
[`current-ecosystem-raw-2026-08-26.csv`](../benchmark-results/current-ecosystem-raw-2026-08-26.csv).

```sh
cmake -S . -B build-ecosystem -DCMAKE_BUILD_TYPE=Release \
  -DMTF_BUILD_ECOSYSTEM_BENCHMARKS=ON
cmake --build build-ecosystem --parallel \
  --target MicroTelemetryFrameworkEcosystemBenchmark
./build-ecosystem/MicroTelemetryFrameworkEcosystemBenchmark 100000
```
