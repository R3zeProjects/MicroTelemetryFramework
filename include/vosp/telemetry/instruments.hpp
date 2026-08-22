#pragma once

/** @file instruments.hpp Thread-safe metric instruments and snapshots. */

#include <vosp/telemetry/record.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace vosp::telemetry
{
namespace detail
{
class InstrumentState
{
public:
    InstrumentState(std::string name, Attributes attributes)
        : name_{std::move(name)}, attributes_{std::move(attributes)}
    {
    }

    virtual ~InstrumentState() noexcept = default;
    [[nodiscard]] virtual Record snapshot() const = 0;

    [[nodiscard]] std::string_view name() const noexcept { return name_; }

protected:
    [[nodiscard]] Record make_record(MetricData data) const
    {
        return Record{name_, std::chrono::system_clock::now(), std::move(data), attributes_};
    }

private:
    std::string name_;
    Attributes attributes_;
};

class CounterState final : public InstrumentState
{
public:
    using InstrumentState::InstrumentState;

    std::atomic<double> value = 0.0;

    [[nodiscard]] Record snapshot() const override
    {
        return make_record(MetricData{
            .kind = MetricKind::COUNTER,
            .value = value.load(std::memory_order_relaxed),
            .count = 0,
            .boundaries = {},
            .buckets = {}});
    }
};

class GaugeState final : public InstrumentState
{
public:
    using InstrumentState::InstrumentState;

    std::atomic<double> value = 0.0;

    [[nodiscard]] Record snapshot() const override
    {
        return make_record(MetricData{
            .kind = MetricKind::GAUGE,
            .value = value.load(std::memory_order_relaxed),
            .count = 0,
            .boundaries = {},
            .buckets = {}});
    }
};

class HistogramState final : public InstrumentState
{
public:
    HistogramState(std::string name, Attributes attributes, std::vector<double> boundaries)
        : InstrumentState{std::move(name), std::move(attributes)},
          boundaries_{std::move(boundaries)}, buckets_(boundaries_.size() + 1)
    {
    }

    [[nodiscard]] bool observe(double value)
    {
        if (!std::isfinite(value))
        {
            return false;
        }
        std::scoped_lock lock{mutex_};
        const auto position = std::ranges::upper_bound(boundaries_, value);
        ++buckets_[static_cast<std::size_t>(position - boundaries_.begin())];
        ++count_;
        sum_ += value;
        return true;
    }

    [[nodiscard]] Record snapshot() const override
    {
        std::scoped_lock lock{mutex_};
        return make_record(MetricData{
            .kind = MetricKind::HISTOGRAM,
            .value = sum_,
            .count = count_,
            .boundaries = boundaries_,
            .buckets = buckets_});
    }

    [[nodiscard]] const std::vector<double>& boundaries() const noexcept
    {
        return boundaries_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<double> boundaries_;
    std::vector<std::uint64_t> buckets_;
    std::uint64_t count_ = 0;
    double sum_ = 0.0;
};
} // namespace detail

/** @brief Cheap shared handle to a monotonic thread-safe metric. */
class Counter
{
public:
    [[nodiscard]] bool add(double delta = 1.0) noexcept
    {
        if (!state_ || !std::isfinite(delta) || delta < 0.0)
        {
            return false;
        }
        state_->value.fetch_add(delta, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] double value() const noexcept
    {
        return state_ ? state_->value.load(std::memory_order_relaxed) : 0.0;
    }

private:
    explicit Counter(std::shared_ptr<detail::CounterState> state) noexcept
        : state_{std::move(state)}
    {
    }
    std::shared_ptr<detail::CounterState> state_;
    friend class Registry;
};

/** @brief Cheap shared handle to a thread-safe instantaneous value. */
class Gauge
{
public:
    [[nodiscard]] bool set(double value) noexcept
    {
        if (!state_ || !std::isfinite(value))
        {
            return false;
        }
        state_->value.store(value, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] bool add(double delta) noexcept
    {
        if (!state_ || !std::isfinite(delta))
        {
            return false;
        }
        state_->value.fetch_add(delta, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] double value() const noexcept
    {
        return state_ ? state_->value.load(std::memory_order_relaxed) : 0.0;
    }

private:
    explicit Gauge(std::shared_ptr<detail::GaugeState> state) noexcept
        : state_{std::move(state)}
    {
    }
    std::shared_ptr<detail::GaugeState> state_;
    friend class Registry;
};

/** @brief Cheap shared handle to a fixed-boundary thread-safe histogram. */
class Histogram
{
public:
    [[nodiscard]] bool observe(double value) { return state_ && state_->observe(value); }

private:
    explicit Histogram(std::shared_ptr<detail::HistogramState> state) noexcept
        : state_{std::move(state)}
    {
    }
    std::shared_ptr<detail::HistogramState> state_;
    friend class Registry;
};
} // namespace vosp::telemetry
