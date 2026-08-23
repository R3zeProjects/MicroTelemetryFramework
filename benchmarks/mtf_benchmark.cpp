#include <vosp/telemetry.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

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

    constexpr std::size_t batch_size = 64;
    std::vector<vosp::telemetry::Record> records;
    records.reserve(batch_size);
    for (std::size_t index = 0; index < batch_size; ++index)
    {
        records.push_back(vosp::telemetry::Record::event("benchmark.batch"));
    }
    auto batch_exporter = std::make_shared<CountingExporter>();
    vosp::telemetry::Pipeline<Policy> batch_pipeline{batch_exporter};
    const auto batch_start = std::chrono::steady_clock::now();
    std::uint64_t published = 0;
    while (published < iterations)
    {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(batch_size, iterations - published));
        static_cast<void>(batch_pipeline.publish(std::span{records}.first(count)));
        published += count;
    }
    static_cast<void>(batch_pipeline.flush());
    const auto batch_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - batch_start).count();

    std::cout << "counter operations/s=" << iterations / counter_seconds << '\n'
              << "async records/s=" << iterations / export_seconds << '\n'
              << "async batch records/s=" << iterations / batch_seconds << '\n'
              << "delivered=" << exporter->count() << '\n'
              << "batch delivered=" << batch_exporter->count() << '\n';
}
