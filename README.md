# MicroTelemetryFramework

MicroTelemetryFramework (MTF) — header-only C++23-фреймворк для сбора и
экспорта метрик, событий и завершённых spans без привязки приложения к
конкретному observability backend.

> Создайте один контур телеметрии вокруг подсистемы и заменяйте exporter без
> переписывания instrumentation.

Версия `0.1.1-beta` поддерживает:

- потокобезопасные `Counter`, `Gauge` и `Histogram`;
- bounded `Registry` с уникальными именами;
- владеющие `Record` для метрик, events и spans;
- прямой и bounded asynchronous pipeline;
- blocking backpressure, batching, `flush` и drain-first `shutdown`;
- `IExporter`, `MemoryExporter` и MCF-контракты;
- отсутствие глобального singleton и обязательного transport.

## Быстрый старт

```cpp
auto exporter = std::make_shared<vosp::telemetry::MemoryExporter>();
vsp::Telemetry telemetry{exporter};
vosp::telemetry::Registry metrics;

auto requests = metrics.counter("http.requests", {{"service", "gateway"}});
auto latency = metrics.histogram("http.latency_ms", {1.0, 5.0, 25.0, 100.0});
requests.add();
latency.observe(4.2);
telemetry.collect(metrics);
telemetry.publish(vosp::telemetry::Record::event("service.ready"));
```

Для фоновой доставки меняется только policy:

```cpp
using Async = vosp::telemetry::pipeline_policy::Async<1024, 64>;
vsp::TelemetryPipeline<Async> telemetry{exporter};
```

## Подключение

MTF зависит только от MCF:

```cmake
find_package(mtf 0.1 REQUIRED CONFIG)
target_link_libraries(application PRIVATE vosp::telemetry)
```

```sh
cmake -S . -B build -DMTF_CONTRACTS_SOURCE_DIR=/path/to/MCF \
  -DBUILD_TESTING=ON -DMTF_BUILD_EXAMPLES=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Полное API показано в [`examples/complete.cpp`](examples/complete.cpp).
Документация: [установка](docs/INSTALLATION.md),
[использование](docs/USAGE_EXAMPLES.md), [контракты](docs/API_CONTRACTS.md),
[архитектура](docs/ARCHITECTURE.md), [бенчмарки](docs/BENCHMARKS.md).

Лицензия MIT.
