#include <vosp/telemetry.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

namespace
{
class CountingExporter final : public vosp::telemetry::IExporter
{
public:
    [[nodiscard]] bool export_batch(std::span<const vosp::telemetry::Record> records) override
    {
        count_.fetch_add(records.size(), std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] std::uint64_t count() const noexcept
    {
        return count_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> count_ = 0;
};
}

int main()
{
    constexpr std::uint64_t iterations = 1'000'000;

    vosp::telemetry::Registry registry;
    auto counter = registry.counter("benchmark.counter");
    const auto counter_start = std::chrono::steady_clock::now();
    for (std::uint64_t index = 0; index < iterations; ++index)
    {
        static_cast<void>(counter.add());
    }
    const auto counter_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - counter_start).count();

    auto exporter = std::make_shared<CountingExporter>();
    using Policy = vosp::telemetry::pipeline_policy::Async<1024, 64>;
    vosp::telemetry::Pipeline<Policy> pipeline{exporter};
    const auto export_start = std::chrono::steady_clock::now();
    for (std::uint64_t index = 0; index < iterations; ++index)
    {
        static_cast<void>(pipeline.publish(vosp::telemetry::Record::event("benchmark.event")));
    }
    static_cast<void>(pipeline.flush());
    const auto export_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - export_start).count();

    std::cout << "counter operations/s=" << iterations / counter_seconds << '\n'
              << "async records/s=" << iterations / export_seconds << '\n'
              << "delivered=" << exporter->count() << '\n';
}
