#pragma once

/** @file record.hpp Owning telemetry values shared with exporters. */

#include <vosp/contracts/telemetry.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace vosp::telemetry
{
/** @brief Owning key/value metadata attached to telemetry. */
struct Attribute
{
    std::string key;
    std::string value;

    [[nodiscard]] bool operator==(const Attribute&) const noexcept = default;
};

using Attributes = std::vector<Attribute>;

/** @brief Metric aggregation represented by a point. */
enum class MetricKind : std::uint8_t
{
    COUNTER,
    GAUGE,
    HISTOGRAM
};

/** @brief Status recorded when a span finishes. */
enum class SpanStatus : std::uint8_t
{
    UNSET,
    OK,
    ERROR
};

/** @brief Snapshot of one metric instrument. */
struct MetricData
{
    MetricKind kind = MetricKind::GAUGE;
    double value = 0.0;
    std::uint64_t count = 0;
    std::vector<double> boundaries;
    std::vector<std::uint64_t> buckets;
};

/** @brief Instantaneous telemetry event. */
struct EventData
{
};

/** @brief Completed trace span. */
struct SpanData
{
    std::chrono::nanoseconds duration{};
    SpanStatus status = SpanStatus::UNSET;
};

using Payload = std::variant<MetricData, EventData, SpanData>;

/** @brief Owning exporter-facing telemetry envelope. */
class Record
{
public:
    Record(std::string name,
           std::chrono::system_clock::time_point timestamp,
           Payload payload,
           Attributes attributes = {})
        : name_{std::move(name)}, timestamp_{timestamp},
          payload_{std::move(payload)}, attributes_{std::move(attributes)}
    {
    }

    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] std::chrono::system_clock::time_point timestamp() const noexcept
    {
        return timestamp_;
    }
    [[nodiscard]] const Payload& payload() const noexcept { return payload_; }
    [[nodiscard]] const Attributes& attributes() const noexcept { return attributes_; }

    /** @brief Creates an instantaneous event using the current wall clock. */
    [[nodiscard]] static Record event(std::string name, Attributes attributes = {})
    {
        return Record{std::move(name), std::chrono::system_clock::now(),
                      EventData{}, std::move(attributes)};
    }

private:
    std::string name_;
    std::chrono::system_clock::time_point timestamp_;
    Payload payload_;
    Attributes attributes_;
};

static_assert(vosp::contracts::TelemetryRecord<Record>);

/** @brief Move-only timer that creates one completed span record. */
class Span
{
public:
    explicit Span(std::string name, Attributes attributes = {})
        : name_{std::move(name)}, attributes_{std::move(attributes)},
          wall_started_{std::chrono::system_clock::now()},
          steady_started_{std::chrono::steady_clock::now()}
    {
    }

    Span(const Span&) = delete;
    Span& operator=(const Span&) = delete;
    Span(Span&& other) noexcept
        : name_{std::move(other.name_)}, attributes_{std::move(other.attributes_)},
          wall_started_{other.wall_started_}, steady_started_{other.steady_started_},
          finished_{other.finished_}
    {
        other.finished_ = true;
    }

    Span& operator=(Span&& other) noexcept
    {
        if (this != &other)
        {
            name_ = std::move(other.name_);
            attributes_ = std::move(other.attributes_);
            wall_started_ = other.wall_started_;
            steady_started_ = other.steady_started_;
            finished_ = other.finished_;
            other.finished_ = true;
        }
        return *this;
    }

    /** @brief Finishes the span. Calling this more than once returns no record. */
    [[nodiscard]] std::optional<Record> finish(SpanStatus status = SpanStatus::OK)
    {
        if (finished_)
        {
            return std::nullopt;
        }
        finished_ = true;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - steady_started_);
        return Record{name_, wall_started_,
                      SpanData{.duration = elapsed, .status = status}, attributes_};
    }

    [[nodiscard]] bool active() const noexcept { return !finished_; }

private:
    std::string name_;
    Attributes attributes_;
    std::chrono::system_clock::time_point wall_started_;
    std::chrono::steady_clock::time_point steady_started_;
    bool finished_ = false;
};
} // namespace vosp::telemetry
