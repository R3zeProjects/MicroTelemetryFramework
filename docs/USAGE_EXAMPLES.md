# Usage examples

The minimal [`examples/basic.cpp`](../examples/basic.cpp) shows common metric
collection. [`examples/complete.cpp`](../examples/complete.cpp) additionally
covers every instrument, events, spans, direct and asynchronous pipelines,
batch publication, lifecycle and statistics.

## Instruments and registry

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

Handles share instrument state and are thread-safe. Names are unique across
instrument kinds. Registry capacity and histogram boundaries are validated at
construction/registration boundaries.

## Events and spans

```cpp
auto event = vosp::telemetry::Record::event(
    "service.ready", {{"node", "one"}});
vosp::telemetry::Span span{"request", {{"route", "/health"}}};
auto completed = span.finish(vosp::telemetry::SpanStatus::OK);
```

`Span` is move-only and can finish once. `Record::payload()` contains
`MetricData`, `EventData`, or `SpanData`; records own their names and attributes.

## Exporters and pipelines

```cpp
class Exporter final : public vosp::telemetry::IExporter
{
public:
    bool export_batch(std::span<const vosp::telemetry::Record> records) override;
};

auto exporter = std::make_shared<Exporter>();
vsp::Telemetry direct{exporter};
direct.publish(event);
direct.publish(records);

using Async = vosp::telemetry::pipeline_policy::Async<1024, 64>;
vsp::TelemetryPipeline<Async> async{exporter};
async.publish(records);
const bool drained = async.flush();
async.shutdown();
const auto stats = async.stats();
```

Async capacity and batch size are compile-time policy values. Producers block
when the bounded queue is full. Shutdown rejects new records, drains accepted
records, and joins the worker. Exporter exceptions become rejected records and
`export_failures`; they do not terminate the worker.

## Stable low-level surface

`Record`, payload structures, instruments, `Registry`, `IExporter`,
`MemoryExporter`, pipeline policies and `PipelineStats` are public. Instrument
state classes are exposed for framework composition but should normally be
obtained through `Registry`. Private queue state is not an extension seam.
