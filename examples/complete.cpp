/** @file
 *  @brief Полный пример инструментов, событий, span и exporter-конвейеров.
 */
#include <vosp/telemetry.hpp>

#include <array>
#include <memory>
#include <variant>

int main()
{
    using namespace vosp::telemetry;

    auto exporter = std::make_shared<MemoryExporter>();
    Registry registry{32};
    auto requests = registry.counter("requests", {{"service", "gateway"}});
    auto active = registry.gauge("active");
    auto latency = registry.histogram("latency_ms", {1.0, 5.0, 25.0, 100.0});
    if (!requests.add(2) || !active.set(3.0) || !active.add(-1.0) || !latency.observe(4.2))
    {
        return 1;
    }

    vsp::Telemetry direct{exporter};
    if (!direct.collect(registry) || !direct.publish(Record::event("service.ready")))
    {
        return 1;
    }

    Span span{"request", {{"route", "/health"}}};
    const auto completed = span.finish(SpanStatus::OK);
    if (!completed || !direct.publish(*completed))
    {
        return 1;
    }

    using Async = pipeline_policy::Async<64, 8>;
    vsp::TelemetryPipeline<Async> asynchronous{exporter};
    const std::array batch{Record::event("one"), Record::event("two")};
    if (!asynchronous.publish(batch) || !asynchronous.flush())
    {
        return 1;
    }
    asynchronous.shutdown();
    const auto stats = asynchronous.stats();
    return stats.accepted == 2 && stats.exported == 2 && stats.rejected == 0 ? 0 : 1;
}
