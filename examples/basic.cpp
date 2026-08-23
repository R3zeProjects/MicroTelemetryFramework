/** @file
 *  @brief Минимальный пример построения telemetry-контура.
 */
#include <vosp/telemetry.hpp>

#include <iostream>
#include <memory>

int main()
{
    auto exporter = std::make_shared<vosp::telemetry::MemoryExporter>();
    vsp::Telemetry telemetry{exporter};
    vosp::telemetry::Registry metrics;

    auto requests = metrics.counter(
        "http.requests",
        {vosp::telemetry::Attribute{.key = "service", .value = "gateway"}});
    auto latency = metrics.histogram("http.latency_ms", {1.0, 5.0, 25.0, 100.0});

    static_cast<void>(requests.add());
    static_cast<void>(latency.observe(4.2));
    static_cast<void>(telemetry.collect(metrics));
    static_cast<void>(telemetry.publish(vosp::telemetry::Record::event("service.ready")));

    std::cout << "exported records: " << exporter->size() << '\n';
}
