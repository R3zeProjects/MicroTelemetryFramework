#pragma once

/** @file exporter.hpp Exporter interface and in-memory implementation. */

#include <vosp/telemetry/record.hpp>

#include <mutex>
#include <span>
#include <vector>

namespace vosp::telemetry
{
/** @brief Runtime-polymorphic destination for batches of telemetry records. */
class IExporter
{
public:
    [[nodiscard]] virtual bool export_batch(std::span<const Record> records) = 0;
    virtual ~IExporter() noexcept = default;
};

static_assert(vosp::contracts::TelemetryExporter<IExporter, Record>);

/** @brief Thread-safe exporter useful for tests and in-process inspection. */
class MemoryExporter final : public IExporter
{
public:
    [[nodiscard]] bool export_batch(std::span<const Record> records) override
    {
        std::scoped_lock lock{mutex_};
        records_.insert(records_.end(), records.begin(), records.end());
        return true;
    }

    [[nodiscard]] std::vector<Record> snapshot() const
    {
        std::scoped_lock lock{mutex_};
        return records_;
    }

    [[nodiscard]] std::size_t size() const
    {
        std::scoped_lock lock{mutex_};
        return records_.size();
    }

    void clear()
    {
        std::scoped_lock lock{mutex_};
        records_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<Record> records_;
};
} // namespace vosp::telemetry
