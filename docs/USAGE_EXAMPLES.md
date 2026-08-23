# Полное использование

[`examples/basic.cpp`](../examples/basic.cpp) показывает основной сценарий, а
[`examples/complete.cpp`](../examples/complete.cpp) — все instruments, события,
spans, direct/async pipelines, batches, lifecycle и статистику.

## Instruments и Registry

```cpp
vosp::telemetry::Registry registry{256};
auto requests = registry.counter("requests", {{"service", "gateway"}});
auto active = registry.gauge("active");
auto latency = registry.histogram("latency_ms", {1.0, 5.0, 25.0, 100.0});
requests.add();
active.set(4.0);
active.add(-1.0);
latency.observe(4.2);
const auto records = registry.collect();
```

Handles разделяют состояние и потокобезопасны. Имена уникальны между видами
instruments. Capacity и границы histogram проверяются явно.

## Events и spans

```cpp
auto event = vosp::telemetry::Record::event("service.ready", {{"node", "one"}});
vosp::telemetry::Span span{"request", {{"route", "/health"}}};
auto completed = span.finish(vosp::telemetry::SpanStatus::OK);
```

`Span` является move-only и завершается один раз. `Record` владеет именем,
attributes и одним из `MetricData`, `EventData`, `SpanData`.

## Exporter и pipelines

```cpp
class Exporter final : public vosp::telemetry::IExporter {
public:
    bool export_batch(std::span<const vosp::telemetry::Record>) override;
};

using Async = vosp::telemetry::pipeline_policy::Async<1024, 64>;
vsp::TelemetryPipeline<Async> pipeline{exporter};
pipeline.publish(records);
pipeline.flush();
pipeline.shutdown();
const auto stats = pipeline.stats();
```

При заполненной очереди producer блокируется. Shutdown отклоняет новые записи,
доставляет принятые и завершает worker. Исключение exporter учитывается в
`rejected` и `export_failures`, не уничтожая worker.

Публичны records, payload types, instruments, `Registry`, exporters, pipeline
policies и `PipelineStats`. Внутренняя очередь не является extension point.
