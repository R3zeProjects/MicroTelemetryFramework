#pragma once

/** @file registry.hpp Owning registry for metric instruments. */

#include <vosp/telemetry/instruments.hpp>

#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vosp::telemetry
{
/** @brief Thread-safe owner and factory for uniquely named instruments. */
class Registry
{
public:
    static constexpr std::size_t default_capacity = 4096;
    static constexpr std::size_t max_capacity = 1'000'000;

    explicit Registry(std::size_t capacity = default_capacity)
        : capacity_{capacity}
    {
        if (capacity == 0 || capacity > max_capacity)
        {
            throw std::invalid_argument{"registry capacity is outside the supported range"};
        }
        instruments_.reserve(std::min(capacity, std::size_t{4096}));
    }

    [[nodiscard]] Counter counter(std::string name, Attributes attributes = {})
    {
        return Counter{get_or_create<detail::CounterState>(
            std::move(name), std::move(attributes))};
    }

    [[nodiscard]] Gauge gauge(std::string name, Attributes attributes = {})
    {
        return Gauge{get_or_create<detail::GaugeState>(
            std::move(name), std::move(attributes))};
    }

    [[nodiscard]] Histogram histogram(
        std::string name,
        std::vector<double> boundaries,
        Attributes attributes = {})
    {
        if (!std::ranges::is_sorted(boundaries) ||
            std::ranges::adjacent_find(boundaries) != boundaries.end() ||
            std::ranges::any_of(boundaries, [](double value) { return !std::isfinite(value); }))
        {
            throw std::invalid_argument{"histogram boundaries must be finite and strictly ordered"};
        }

        std::scoped_lock lock{mutex_};
        validate_name(name);
        if (const auto iterator = instruments_.find(name); iterator != instruments_.end())
        {
            auto state = std::dynamic_pointer_cast<detail::HistogramState>(iterator->second);
            if (!state || state->boundaries() != boundaries)
            {
                throw std::logic_error{"instrument name already uses another definition"};
            }
            return Histogram{std::move(state)};
        }
        ensure_space();
        auto state = std::make_shared<detail::HistogramState>(
            name, std::move(attributes), std::move(boundaries));
        instruments_.emplace(std::move(name), state);
        return Histogram{std::move(state)};
    }

    /** @brief Captures owning snapshots without holding the registry lock during export. */
    [[nodiscard]] std::vector<Record> collect() const
    {
        std::vector<std::shared_ptr<detail::InstrumentState>> states;
        {
            std::scoped_lock lock{mutex_};
            states.reserve(instruments_.size());
            for (const auto& state : instruments_ | std::views::values)
            {
                states.push_back(state);
            }
        }

        std::vector<Record> records;
        records.reserve(states.size());
        for (const auto& state : states)
        {
            records.push_back(state->snapshot());
        }
        return records;
    }

    [[nodiscard]] bool remove(std::string_view name)
    {
        std::scoped_lock lock{mutex_};
        return instruments_.erase(std::string{name}) != 0;
    }

    [[nodiscard]] std::size_t size() const
    {
        std::scoped_lock lock{mutex_};
        return instruments_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    void clear()
    {
        std::scoped_lock lock{mutex_};
        instruments_.clear();
    }

private:
    static void validate_name(std::string_view name)
    {
        if (name.empty())
        {
            throw std::invalid_argument{"instrument name must not be empty"};
        }
    }

    void ensure_space() const
    {
        if (instruments_.size() >= capacity_)
        {
            throw std::length_error{"instrument registry capacity reached"};
        }
    }

    template<typename State>
    [[nodiscard]] std::shared_ptr<State> get_or_create(
        std::string name,
        Attributes attributes)
    {
        std::scoped_lock lock{mutex_};
        validate_name(name);
        if (const auto iterator = instruments_.find(name); iterator != instruments_.end())
        {
            auto state = std::dynamic_pointer_cast<State>(iterator->second);
            if (!state)
            {
                throw std::logic_error{"instrument name already uses another kind"};
            }
            return state;
        }
        ensure_space();
        auto state = std::make_shared<State>(name, std::move(attributes));
        instruments_.emplace(std::move(name), state);
        return state;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<detail::InstrumentState>> instruments_;
    std::size_t capacity_;
};
} // namespace vosp::telemetry
