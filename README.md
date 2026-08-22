# MicroTelemetryFramework

MicroTelemetryFramework (MTF) is a small C++23 framework for collecting and
exporting metrics, events, and completed spans without coupling application
code to a specific observability backend.

> Build one telemetry contour around any subsystem, then replace its exporter
> without rewriting instrumentation.

The current release is **0.1.0-beta**. MTF is header-only and supports GCC,
Clang, and MSVC through CMake 3.25 or newer.

## Capabilities

- atomic thread-safe `Counter` and `Gauge` handles;
- fixed-boundary `Histogram` with consistent snapshots;
- bounded, thread-safe `Registry` with unique instrument names;
- owning `Record` values for metrics, events, and completed spans;
- synchronous export with no framework queue;
- bounded asynchronous export with blocking backpressure and batching;
- explicit `flush()`, drain-first `shutdown()`, and delivery counters;
- runtime-polymorphic `IExporter` and a thread-safe `MemoryExporter`;
- compile-time protocol validation through MicroContractsFramework;
- no dependency on a transport, database, logging backend, or global singleton.

## Quick start

```cpp
#include <vosp/telemetry.hpp>

#include <memory>

int main()
{
    auto exporter = std::make_shared<vosp::telemetry::MemoryExporter>();
    vsp::Telemetry telemetry{exporter};
    vosp::telemetry::Registry metrics;

    auto requests = metrics.counter("http.requests", {{"service", "gateway"}});
    auto latency = metrics.histogram("http.latency_ms", {1.0, 5.0, 25.0, 100.0});

    static_cast<void>(requests.add());
    static_cast<void>(latency.observe(4.2));
    static_cast<void>(telemetry.collect(metrics));
    static_cast<void>(telemetry.publish(
        vosp::telemetry::Record::event("service.ready")));
}
```

For bounded background export, change only the policy:

```cpp
using Async = vosp::telemetry::pipeline_policy::Async<1024, 64>;
vsp::TelemetryPipeline<Async> telemetry{exporter};
```

The first parameter is queue capacity and the second is maximum export batch
size. Producers block while the queue is full; accepted records are drained
before shutdown completes.

## Dependency

MTF depends on
[MicroContractsFramework](https://github.com/R3zeProjects/MicroContractsFramework)
`0.5.x` for structural telemetry concepts. MCF owns no telemetry runtime state.
MEF and MPF are not required by the MTF core.

```cmake
find_package(mtf 0.1 REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE vosp::telemetry)
```

For source-tree development:

```sh
cmake -S . -B build \
  -DMTF_CONTRACTS_SOURCE_DIR=/path/to/MicroContractsFramework \
  -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Without `MTF_CONTRACTS_SOURCE_DIR`, CMake first searches for an installed
`vosp_contracts 0.5` package and otherwise fetches the pinned compatible commit.

## Measured baseline

Local Release results on an AMD Ryzen 7 PRO 1700X (8 cores / 16 threads),
Clang 22.1.6, Windows. Median of five runs, 1,000,000 operations per run:

| Scenario | Median throughput |
| --- | ---: |
| Atomic counter update | **119.593M operations/s** |
| Bounded async event export, q1024/b64 | **1.985M records/s** |

The exporter only counts delivered records, so the async result measures MTF
record construction, queueing, synchronization, batching, and delivery. These
numbers are a reproducible baseline, not a cross-machine guarantee. Benchmarks
are repository tooling and are not installed with the package.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DMTF_BUILD_BENCHMARKS=ON
cmake --build build-release --parallel
./build-release/MicroTelemetryFrameworkBenchmark
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [API and concurrency contracts](docs/API_CONTRACTS.md)
- [Benchmark methodology](docs/BENCHMARKS.md)

MTF is licensed under the MIT License.
