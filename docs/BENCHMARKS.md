# Benchmark methodology

`MicroTelemetryFrameworkBenchmark` is an opt-in repository target. It is never
installed or exported by the CMake package.

The initial baseline executes two one-million-operation scenarios:

1. relaxed atomic increments through a copied `Counter` handle;
2. construction and publication of owning event records through an async
   pipeline configured with capacity 1024 and batch size 64.

The async exporter performs one relaxed atomic addition per batch and verifies
that all records were delivered. Timing includes queue contention,
backpressure, record moves, worker wakeups, and exporter invocation. It excludes
process startup and pipeline construction.

Results in the README are the median of five consecutive Release runs. Compare
versions only with the same compiler, machine, power plan, iteration count, and
benchmark source. Debug numbers and results from different machines must not be
presented as regressions or improvements.

## External comparison

`MicroTelemetryFrameworkExternalBenchmark` is enabled by
`MTF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS=ON`. CMake fetches immutable commits:

- prometheus-cpp 1.3.0: `e5fada43131d251e9c4786b04263ce98b6767ba5`;
- OpenTelemetry C++ 1.9.1: `770fce3c4095f6dc852fd80fb0810936b723be9a`.

Each timed counter loop adds `1.0` to one unlabelled instrument. Each histogram
loop observes `4.2`; MTF and prometheus-cpp use boundaries 1, 5, 25, and 100.
OpenTelemetry uses its SDK default histogram aggregation. Instrument creation
and final collection are excluded. MTF, prometheus-cpp, and OpenTelemetry
collection are validated after timing. The atomic baseline quantifies the
minimum synchronization cost on this machine.

The recorded values are seven-run medians from a Clang 22.1.6 Release build on
Windows and AMD Ryzen 7 PRO 1700X. The dataset is
[`v0.1.1-external-medians-2026-08-22.csv`](../benchmark-results/v0.1.1-external-medians-2026-08-22.csv).

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

- MCF `b9f78d3f529097ac1dae963b06274a6110b39c1a`;
- MEF `c11c3aa25814baa1a889ab1f80b718e59e3a24a9`;
- MPF `00aec475b283812c232758c539e70ce8fae09f64`;
- the checked-out MTF source.

One transaction creates one MEF log and one MTF event, updates one counter, and
stores both owning records in a pre-reserved thread-safe in-memory MPF target.
The timer includes value construction, logger dispatch, conversion, queueing
where selected, and persistence copies. It excludes setup and final validation.

Seven-run medians were measured with the same compiler and machine and are stored in
[`v0.1.1-ecosystem-medians-2026-08-22.csv`](../benchmark-results/v0.1.1-ecosystem-medians-2026-08-22.csv).

```sh
cmake -S . -B build-ecosystem -DCMAKE_BUILD_TYPE=Release \
  -DMTF_BUILD_ECOSYSTEM_BENCHMARKS=ON
cmake --build build-ecosystem --parallel \
  --target MicroTelemetryFrameworkEcosystemBenchmark
./build-ecosystem/MicroTelemetryFrameworkEcosystemBenchmark 100000
```
