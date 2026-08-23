# Примеры использования

Минимальный [`examples/basic.cpp`](../examples/basic.cpp) показывает сбор общих метрик. [`examples/complete.cpp`](../examples/complete.cpp) дополнительно охватывает каждый
инструмент, события, spans, прямые и асинхронные pipelines, пакетную публикацию,
жизненный цикл и статистику.

## Инструменты и registry

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

Обрабатывает состояние инструмента доли и является thread-safe. Имена уникальны среди
типов инструментов. Вместимость Registry и границы гистограммы проверяются на границах
конструкции/регистрации.

## События и spans

```cpp
auto event = vosp::telemetry::Record::event(
    "service.ready", {{"node", "one"}});
vosp::telemetry::Span span{"request", {{"route", "/health"}}};
auto completed = span.finish(vosp::telemetry::SpanStatus::OK);
```

`Span` можно только перемещать и он может завершиться только один раз. `Record::payload()` содержит `MetricData`, `EventData` или `SpanData`; записи содержат
свои имена и атрибуты.

## Экспортеры и pipelines

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
async.flush();
async.shutdown();
const auto stats = async.stats();
```

Асинхронная емкость и размер пакета являются значениями политики compile-time.
Производители блокируются, когда ограниченная очередь полна. Завершение работы отклоняет
новые записи, обрабатывает принятые записи и присоединяется к worker. Исключения
экспортера становятся отклоненными записями и `export_failures`; они не завершают
worker.

## Стабильная низкая поверхность

`Record`, структуры полезной нагрузки, инструменты, `Registry`, `IExporter`, `MemoryExporter`, pipeline политики и `PipelineStats` являются общедоступными. Классы
состояния инструментов открыты для фреймворк композиции, но обычно должны получаться
через `Registry`. Частное состояние очереди не является местом расширения.
