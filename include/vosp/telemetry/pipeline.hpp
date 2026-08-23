#pragma once

/** @file pipeline.hpp Direct and bounded asynchronous exporter pipelines. */

#include <vosp/telemetry/exporter.hpp>
#include <vosp/telemetry/registry.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace vosp::telemetry
{
namespace pipeline_policy
{
/** @brief Exports on the calling thread without queueing. */
struct Direct
{
};

/** @brief Exports bounded batches on one background worker. */
template <std::size_t Capacity = 1024, std::size_t BatchSize = 64> struct Async
{
    static_assert(Capacity > 0, "pipeline capacity must be positive");
    static_assert(BatchSize > 0, "pipeline batch size must be positive");
    static constexpr std::size_t capacity = Capacity;
    static constexpr std::size_t batch_size = BatchSize;
};
} // namespace pipeline_policy

/** @brief Observable pipeline counters. */
struct PipelineStats
{
    std::uint64_t accepted = 0;
    std::uint64_t exported = 0;
    std::uint64_t rejected = 0;
    std::uint64_t export_failures = 0;
};

template <typename Policy = pipeline_policy::Direct> class Pipeline;

/** @brief Calling-thread exporter pipeline. */
template <> class Pipeline<pipeline_policy::Direct>
{
public:
    explicit Pipeline(std::shared_ptr<IExporter> exporter) : exporter_{std::move(exporter)}
    {
        if (!exporter_)
        {
            throw std::invalid_argument{"exporter must not be null"};
        }
    }

    [[nodiscard]] bool publish(const Record &record)
    {
        return publish(std::span{&record, std::size_t{1}});
    }

    [[nodiscard]] bool publish(std::span<const Record> records)
    {
        accepted_.fetch_add(records.size(), std::memory_order_relaxed);
        try
        {
            if (exporter_->export_batch(records))
            {
                exported_.fetch_add(records.size(), std::memory_order_relaxed);
                return true;
            }
        }
        catch (...)
        {
            // Exporters are failure boundaries; failures are reported in stats.
            static_cast<void>(0);
        }
        rejected_.fetch_add(records.size(), std::memory_order_relaxed);
        export_failures_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    [[nodiscard]] bool collect(const Registry &registry)
    {
        const auto records = registry.collect();
        return publish(records);
    }

    [[nodiscard]] bool flush() const noexcept { return true; }
    void shutdown() const noexcept {}

    [[nodiscard]] PipelineStats stats() const noexcept
    {
        return {.accepted = accepted_.load(std::memory_order_relaxed),
                .exported = exported_.load(std::memory_order_relaxed),
                .rejected = rejected_.load(std::memory_order_relaxed),
                .export_failures = export_failures_.load(std::memory_order_relaxed)};
    }

private:
    std::shared_ptr<IExporter> exporter_;
    std::atomic<std::uint64_t> accepted_ = 0;
    std::atomic<std::uint64_t> exported_ = 0;
    std::atomic<std::uint64_t> rejected_ = 0;
    std::atomic<std::uint64_t> export_failures_ = 0;
};

/** @brief Bounded blocking pipeline with one batching exporter worker. */
template <std::size_t Capacity, std::size_t BatchSize>
class Pipeline<pipeline_policy::Async<Capacity, BatchSize>>
{
private:
    struct State
    {
        explicit State(std::shared_ptr<IExporter> destination) : exporter{std::move(destination)}
        {
            batch.reserve(BatchSize);
        }

        std::shared_ptr<IExporter> exporter;
        std::mutex mutex;
        std::condition_variable work_available;
        std::condition_variable space_available;
        std::condition_variable idle;
        std::deque<Record> queue;
        std::vector<Record> batch;
        std::size_t in_flight = 0;
        bool stopping = false;
        bool stopped = false;
        std::thread::id worker_id{};
        std::uint64_t accepted = 0;
        std::uint64_t exported = 0;
        std::uint64_t rejected = 0;
        std::uint64_t export_failures = 0;
    };

public:
    explicit Pipeline(std::shared_ptr<IExporter> exporter)
        : state_{std::make_shared<State>(std::move(exporter))},
          worker_{[state = state_] { run(state); }}
    {
        if (!state_->exporter)
        {
            {
                std::scoped_lock lock{state_->mutex};
                state_->stopping = true;
            }
            state_->work_available.notify_all();
            worker_.join();
            throw std::invalid_argument{"exporter must not be null"};
        }
    }

    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;
    Pipeline(Pipeline &&) = delete;
    Pipeline &operator=(Pipeline &&) = delete;

    ~Pipeline() noexcept { shutdown(); }

    /** @brief Enqueues an owned record, blocking while the bounded queue is full.
     */
    [[nodiscard]] bool publish(Record record)
    {
        std::unique_lock lock{state_->mutex};
        state_->space_available.wait(
            lock, [this] { return state_->stopping || state_->queue.size() < Capacity; });
        if (state_->stopping)
        {
            ++state_->rejected;
            return false;
        }
        state_->queue.push_back(std::move(record));
        ++state_->accepted;
        lock.unlock();
        state_->work_available.notify_one();
        return true;
    }

    [[nodiscard]] bool publish(std::span<const Record> records)
    {
        std::size_t offset = 0;
        while (offset < records.size())
        {
            std::unique_lock lock{state_->mutex};
            state_->space_available.wait(lock, [this]
            {
                return state_->stopping || state_->queue.size() < Capacity;
            });
            if (state_->stopping)
            {
                state_->rejected += records.size() - offset;
                return false;
            }

            const auto count = std::min(records.size() - offset,
                                        Capacity - state_->queue.size());
            const auto chunk = records.subspan(offset, count);
            for (const auto& record : chunk)
            {
                state_->queue.push_back(record);
            }
            state_->accepted += count;
            offset += count;
            lock.unlock();
            state_->work_available.notify_one();
        }
        return true;
    }

    [[nodiscard]] bool collect(const Registry &registry)
    {
        const auto records = registry.collect();
        return publish(records);
    }

    /** @brief Waits until every accepted record has completed export. */
    [[nodiscard]] bool flush()
    {
        std::unique_lock lock{state_->mutex};
        if (state_->worker_id == std::this_thread::get_id())
        {
            return false;
        }
        state_->idle.wait(
            lock, [this]
            { return (state_->queue.empty() && state_->in_flight == 0) || state_->stopped; });
        return state_->queue.empty() && state_->in_flight == 0;
    }

    /** @brief Stops submission, drains accepted records, and joins the worker. */
    void shutdown() noexcept
    {
        std::scoped_lock shutdown_lock{shutdown_mutex_};
        const auto state = state_;
        {
            std::scoped_lock lock{state->mutex};
            state->stopping = true;
        }
        state->work_available.notify_all();
        state->space_available.notify_all();
        if (worker_.joinable())
        {
            if (worker_.get_id() == std::this_thread::get_id())
            {
                worker_.detach();
            }
            else
            {
                worker_.join();
            }
        }
    }

    [[nodiscard]] PipelineStats stats() const noexcept
    {
        std::scoped_lock lock{state_->mutex};
        return {.accepted = state_->accepted,
                .exported = state_->exported,
                .rejected = state_->rejected,
                .export_failures = state_->export_failures};
    }

private:
    static void run(const std::shared_ptr<State> &state) noexcept
    {
        {
            std::scoped_lock lock{state->mutex};
            state->worker_id = std::this_thread::get_id();
        }
        auto &batch = state->batch;
        for (;;)
        {
            {
                std::unique_lock lock{state->mutex};
                state->work_available.wait(lock, [&]
                                           { return state->stopping || !state->queue.empty(); });
                if (state->queue.empty() && state->stopping)
                {
                    state->stopped = true;
                    lock.unlock();
                    state->idle.notify_all();
                    return;
                }

                batch.clear();
                const auto count = std::min(BatchSize, state->queue.size());
                for (std::size_t index = 0; index < count; ++index)
                {
                    batch.push_back(std::move(state->queue.front()));
                    state->queue.pop_front();
                }
                state->in_flight += batch.size();
            }
            state->space_available.notify_all();

            bool exported = false;
            try
            {
                exported = state->exporter->export_batch(batch);
            }
            catch (...)
            {
                // The worker survives exporter failures and records the failed batch.
                exported = false;
            }
            {
                std::scoped_lock lock{state->mutex};
                if (exported)
                {
                    state->exported += batch.size();
                }
                else
                {
                    state->rejected += batch.size();
                    ++state->export_failures;
                }
                state->in_flight -= batch.size();
                if (state->queue.empty() && state->in_flight == 0)
                {
                    state->idle.notify_all();
                }
            }
        }
    }

    std::shared_ptr<State> state_;
    std::thread worker_;
    std::mutex shutdown_mutex_;
};
} // namespace vosp::telemetry
