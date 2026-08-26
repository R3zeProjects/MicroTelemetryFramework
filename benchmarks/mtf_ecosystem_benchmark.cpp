#include <vosp/error.hpp>
#include <vosp/logger.hpp>
#include <vosp/persistence.hpp>
#include <vosp/telemetry.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

class MemoryJournal
{
public:
    explicit MemoryJournal(std::size_t capacity)
    {
        records_.reserve(capacity);
    }

    [[nodiscard]] vosp::persistence::OperationResult append(
        const vosp::persistence::Record& record)
    {
        std::scoped_lock lock{mutex_};
        records_.push_back(record);
        return {};
    }

    [[nodiscard]] std::size_t size() const
    {
        std::scoped_lock lock{mutex_};
        return records_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<vosp::persistence::Record> records_;
};

class PersistenceExporter final : public vosp::telemetry::IExporter
{
public:
    explicit PersistenceExporter(MemoryJournal& journal) noexcept
        : journal_{&journal}
    {
    }

    [[nodiscard]] bool export_batch(
        std::span<const vosp::telemetry::Record> records) override
    {
        for (const auto& record : records)
        {
            if (!journal_->append(vosp::persistence::Record{
                    .timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        record.timestamp().time_since_epoch()).count(),
                    .thread_token = 0,
                    .level = static_cast<std::uint32_t>(vosp::logger::Level::INFO),
                    .category = static_cast<std::uint32_t>(vosp::error::Category::NONE),
                    .code = 0,
                    .message = std::string{record.name()}}))
            {
                return false;
            }
        }
        return true;
    }

private:
    MemoryJournal* journal_;
};

static_assert(vosp::contracts::TelemetryExporter<
              PersistenceExporter, vosp::telemetry::Record>);

template<typename Pipeline>
[[nodiscard]] double run_contour(
    std::string_view scenario,
    std::uint64_t operations,
    Pipeline& telemetry,
    MemoryJournal& journal)
{
    vosp::persistence::Sink persistence_sink{journal};
    vosp::logger::Logger logger{persistence_sink};
    vosp::telemetry::Registry metrics;
    auto requests = metrics.counter("ecosystem.requests");
    const vosp::error::Error error{
        vosp::error::Category::NETWORK, 1001, "upstream unavailable"};

    const auto started = Clock::now();
    for (std::uint64_t index = 0; index < operations; ++index)
    {
        if (!logger.error(error) || !requests.add() ||
            !telemetry.publish(vosp::telemetry::Record::event("request.failed")))
        {
            throw std::runtime_error{"ecosystem operation failed"};
        }
    }
    if (!telemetry.flush())
    {
        throw std::runtime_error{"telemetry flush failed"};
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - started).count();

    const auto telemetry_stats = telemetry.stats();
    if (requests.value() != operations ||
        persistence_sink.accepted() != operations ||
        telemetry_stats.exported != operations ||
        journal.size() != operations * 2)
    {
        throw std::runtime_error{"ecosystem delivery validation failed"};
    }

    const auto rate = static_cast<double>(operations) / elapsed;
    std::cout << scenario << ',' << operations << ',' << rate << ','
              << journal.size() << '\n';
    return rate;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::uint64_t operations = argc > 1
            ? std::stoull(argv[1])
            : 100'000;
        std::cout << "scenario,transactions,throughput_per_second,persisted_records\n";

        MemoryJournal direct_journal{static_cast<std::size_t>(operations * 2)};
        auto direct_exporter = std::make_shared<PersistenceExporter>(direct_journal);
        vosp::Telemetry direct{direct_exporter};
        static_cast<void>(run_contour(
            "ecosystem_direct", operations, direct, direct_journal));

        MemoryJournal async_journal{static_cast<std::size_t>(operations * 2)};
        auto async_exporter = std::make_shared<PersistenceExporter>(async_journal);
        using Async = vosp::telemetry::pipeline_policy::Async<1024, 64>;
        vosp::TelemetryPipeline<Async> asynchronous{async_exporter};
        static_cast<void>(run_contour(
            "ecosystem_async", operations, asynchronous, async_journal));
        asynchronous.shutdown();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "benchmark failed: " << exception.what() << '\n';
        return 1;
    }
}
