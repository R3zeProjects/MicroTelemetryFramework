#include <vosp/telemetry.hpp>

#include <memory>

int main()
{
    auto exporter = std::make_shared<vosp::telemetry::MemoryExporter>();
    vsp::Telemetry telemetry{exporter};
    vosp::telemetry::Registry registry;
    auto counter = registry.counter("consumer.requests");
    return counter.add() && telemetry.collect(registry) && exporter->size() == 1 ? 0 : 1;
}
