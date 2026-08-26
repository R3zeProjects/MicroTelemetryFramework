#include <vosp/telemetry.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <variant>
#include <vector>

namespace
{
using namespace vosp::telemetry;

[[nodiscard]] bool check(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

[[nodiscard]] bool instruments_are_thread_safe()
{
    Registry registry;
    auto counter = registry.counter("requests", {Attribute{.key = "service", .value = "api"}});
    auto same_counter = registry.counter("requests");
    constexpr std::size_t workers = 4;
    constexpr std::size_t iterations = 25'000;
    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (std::size_t worker = 0; worker < workers; ++worker)
    {
        threads.emplace_back(
            [counter]() mutable
            {
                for (std::size_t index = 0; index < iterations; ++index)
                {
                    static_cast<void>(counter.add());
                }
            });
    }
    for (auto &thread : threads)
    {
        thread.join();
    }

    auto gauge = registry.gauge("load");
    const bool gauge_set = gauge.set(0.75);
    auto histogram = registry.histogram("latency_ms", {1.0, 10.0, 100.0});
    const bool observations =
        histogram.observe(0.5) && histogram.observe(10.0) && histogram.observe(250.0);
    const auto records = registry.collect();

    const auto histogram_record = std::ranges::find_if(records, [](const Record &record)
                                                       { return record.name() == "latency_ms"; });
    const auto *data = histogram_record == records.end()
                           ? nullptr
                           : std::get_if<MetricData>(&histogram_record->payload());

    return check(counter.value() == workers * iterations, "concurrent counter") &&
           check(same_counter.value() == counter.value(), "shared named instrument") &&
           check(!counter.add(-1.0), "counter rejects negative delta") &&
           check(gauge_set && gauge.value() == 0.75, "gauge update") &&
           check(observations, "histogram observations") &&
           check(data != nullptr && data->count == 3 && data->buckets.size() == 4,
                 "histogram snapshot") &&
           check(records.size() == 3, "registry collection");
}

[[nodiscard]] bool registry_rejects_conflicts()
{
    Registry registry;
    static_cast<void>(registry.counter("same"));
    const std::string_view same = "same";
    try
    {
        static_cast<void>(registry.gauge("same"));
    }
    catch (const std::logic_error &)
    {
        return check(registry.remove(same), "heterogeneous registry removal") &&
               check(!registry.remove(same), "missing registry removal") &&
               check(registry.size() == 0, "registry is empty after removal");
    }
    return false;
}

[[nodiscard]] bool registry_enforces_bounds()
{
    Registry registry{1};
    static_cast<void>(registry.counter("first"));
    bool capacity_rejected = false;
    bool boundaries_rejected = false;
    try
    {
        static_cast<void>(registry.gauge("second"));
    }
    catch (const std::length_error &)
    {
        capacity_rejected = true;
    }
    try
    {
        Registry another;
        static_cast<void>(another.histogram("invalid", {10.0, 1.0}));
    }
    catch (const std::invalid_argument &)
    {
        boundaries_rejected = true;
    }
    return check(capacity_rejected, "registry capacity") &&
           check(boundaries_rejected, "histogram boundary validation");
}

[[nodiscard]] bool records_and_spans_work()
{
    const auto event = Record::event("startup", {Attribute{.key = "node", .value = "one"}});
    Span span{"request"};
    const auto completed = span.finish(SpanStatus::OK);
    const bool span_payload =
        completed.has_value() && std::holds_alternative<SpanData>(completed->payload());
    return check(event.name() == "startup", "event name") &&
           check(std::holds_alternative<EventData>(event.payload()), "event payload") &&
           check(completed.has_value(), "span completion") && check(span_payload, "span payload") &&
           check(!span.finish().has_value(), "span completes once");
}

[[nodiscard]] bool direct_pipeline_works()
{
    auto exporter = std::make_shared<MemoryExporter>();
    vosp::Telemetry pipeline{exporter};
    Registry registry;
    auto counter = registry.counter("jobs");
    static_cast<void>(counter.add(3));
    const auto event = Record::event("ready");
    return check(pipeline.collect(registry), "direct metric export") &&
           check(pipeline.publish(event), "direct event export") &&
           check(exporter->size() == 2, "direct exporter size") &&
           check(pipeline.stats().exported == 2, "direct stats");
}

[[nodiscard]] bool async_pipeline_drains()
{
    auto exporter = std::make_shared<MemoryExporter>();
    using Policy = pipeline_policy::Async<8, 3>;
    vosp::TelemetryPipeline<Policy> pipeline{exporter};
    for (std::size_t index = 0; index < 100; ++index)
    {
        if (!pipeline.publish(Record::event(
                "queued", {Attribute{.key = "index", .value = std::to_string(index)}})))
        {
            return check(false, "async acceptance");
        }
    }
    std::vector<Record> batch;
    batch.reserve(17);
    for (std::size_t index = 0; index < 17; ++index)
    {
        batch.push_back(Record::event("queued batch"));
    }
    if (!pipeline.publish(batch))
    {
        return check(false, "async batch acceptance");
    }
    const bool flushed = pipeline.flush();
    pipeline.shutdown();
    const bool rejected_single = !pipeline.publish(Record::event("late"));
    const std::array rejected_batch{
        Record::event("late batch"), Record::event("late batch"), Record::event("late batch")};
    const bool rejected_all = !pipeline.publish(rejected_batch);
    const auto stats = pipeline.stats();
    return check(flushed, "async flush") && check(exporter->size() == 117, "async drain") &&
           check(stats.accepted == 117 && stats.exported == 117, "async stats") &&
           check(rejected_single && rejected_all, "shutdown rejects work") &&
           check(stats.rejected == 4, "shutdown rejection stats");
}

[[nodiscard]] bool async_pipeline_accepts_concurrent_producers()
{
    auto exporter = std::make_shared<MemoryExporter>();
    using Policy = pipeline_policy::Async<32, 8>;
    vosp::TelemetryPipeline<Policy> pipeline{exporter};
    constexpr std::size_t producers = 4;
    constexpr std::size_t records_per_producer = 2'000;
    std::vector<std::thread> threads;
    threads.reserve(producers);
    std::atomic<bool> accepted = true;
    for (std::size_t producer = 0; producer < producers; ++producer)
    {
        threads.emplace_back(
            [&pipeline, &accepted]
            {
                for (std::size_t index = 0; index < records_per_producer; ++index)
                {
                    if (!pipeline.publish(Record::event("concurrent")))
                    {
                        accepted.store(false, std::memory_order_relaxed);
                        return;
                    }
                }
            });
    }
    for (auto &thread : threads)
    {
        thread.join();
    }
    const bool flushed = pipeline.flush();
    pipeline.shutdown();
    pipeline.shutdown();
    return check(accepted.load(std::memory_order_relaxed), "concurrent acceptance") &&
           check(flushed, "concurrent flush") &&
           check(exporter->size() == producers * records_per_producer, "concurrent delivery");
}
} // namespace

int main()
{
    return instruments_are_thread_safe() && registry_rejects_conflicts() &&
                   registry_enforces_bounds() && records_and_spans_work() &&
                   direct_pipeline_works() && async_pipeline_drains() &&
                   async_pipeline_accepts_concurrent_producers()
               ? 0
               : 1;
}
