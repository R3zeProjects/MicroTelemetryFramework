#include <vosp/telemetry.hpp>

#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/metric_reader.h>
#include <opentelemetry/context/context.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

#include <chrono>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

class OTelReader final : public opentelemetry::sdk::metrics::MetricReader
{
public:
    [[nodiscard]] opentelemetry::sdk::metrics::AggregationTemporality
    GetAggregationTemporality(opentelemetry::sdk::metrics::InstrumentType) const
        noexcept override
    {
        return opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
    }

private:
    [[nodiscard]] bool OnForceFlush(std::chrono::microseconds) noexcept override
    {
        return true;
    }
    [[nodiscard]] bool OnShutDown(std::chrono::microseconds) noexcept override
    {
        return true;
    }
    void OnInitialized() noexcept override {}
};

template<typename Operation>
[[nodiscard]] double measure(std::uint64_t operations, Operation&& operation)
{
    const auto started = Clock::now();
    for (std::uint64_t index = 0; index < operations; ++index)
    {
        operation();
    }
    return static_cast<double>(operations) /
        std::chrono::duration<double>(Clock::now() - started).count();
}

void counter_benchmark(std::uint64_t operations)
{
    std::atomic<double> baseline = 0.0;
    const auto atomic_rate = measure(operations, [&]
    {
        baseline.fetch_add(1.0, std::memory_order_relaxed);
    });

    vosp::telemetry::Registry mtf_registry;
    auto mtf_counter = mtf_registry.counter("requests_total");
    const auto mtf_rate = measure(operations, [&]
    {
        static_cast<void>(mtf_counter.add());
    });

    prometheus::Registry prometheus_registry;
    auto& prometheus_family = prometheus::BuildCounter()
        .Name("requests_total")
        .Help("equal-work counter")
        .Register(prometheus_registry);
    auto& prometheus_counter = prometheus_family.Add({});
    const auto prometheus_rate = measure(operations, [&]
    {
        prometheus_counter.Increment();
    });

    opentelemetry::sdk::metrics::MeterProvider otel_provider;
    auto otel_reader = std::make_shared<OTelReader>();
    otel_provider.AddMetricReader(otel_reader);
    auto otel_meter = otel_provider.GetMeter("mtf-comparison", "1.0");
    auto otel_counter = otel_meter->CreateDoubleCounter("requests_total");
    const auto otel_rate = measure(operations, [&]
    {
        otel_counter->Add(1.0);
    });
    const bool otel_collected = otel_reader->Collect([](auto& resource)
    {
        return !resource.scope_metric_data_.empty() &&
               !resource.scope_metric_data_.front().metric_data_.empty();
    });

    if (baseline.load(std::memory_order_relaxed) != operations ||
        mtf_counter.value() != operations || prometheus_counter.Value() != operations ||
        !otel_collected)
    {
        throw std::runtime_error{"counter delivery validation failed"};
    }

    std::cout << "atomic_baseline,counter," << operations << ',' << atomic_rate << '\n'
              << "mtf_0.1.1,counter," << operations << ',' << mtf_rate << '\n'
              << "prometheus_cpp_1.3.0,counter," << operations << ','
              << prometheus_rate << '\n'
              << "opentelemetry_cpp_1.9.1,counter," << operations << ','
              << otel_rate << '\n';
}

void histogram_benchmark(std::uint64_t operations)
{
    const std::vector<double> boundaries{1.0, 5.0, 25.0, 100.0};
    vosp::telemetry::Registry mtf_registry;
    auto mtf_histogram = mtf_registry.histogram("latency_ms", boundaries);
    const auto mtf_rate = measure(operations, [&]
    {
        static_cast<void>(mtf_histogram.observe(4.2));
    });

    prometheus::Histogram prometheus_histogram{boundaries};
    const auto prometheus_rate = measure(operations, [&]
    {
        prometheus_histogram.Observe(4.2);
    });

    opentelemetry::sdk::metrics::MeterProvider otel_provider;
    auto otel_reader = std::make_shared<OTelReader>();
    otel_provider.AddMetricReader(otel_reader);
    auto otel_meter = otel_provider.GetMeter("mtf-comparison", "1.0");
    auto otel_histogram = otel_meter->CreateDoubleHistogram("latency_ms");
    const auto otel_rate = measure(operations, [&]
    {
        otel_histogram->Record(4.2, opentelemetry::context::Context{});
    });
    const bool otel_collected = otel_reader->Collect([](auto& resource)
    {
        return !resource.scope_metric_data_.empty() &&
               !resource.scope_metric_data_.front().metric_data_.empty();
    });

    const auto mtf_records = mtf_registry.collect();
    const auto* mtf_data = mtf_records.size() == 1
        ? std::get_if<vosp::telemetry::MetricData>(&mtf_records.front().payload())
        : nullptr;
    if (mtf_data == nullptr || mtf_data->count != operations ||
        prometheus_histogram.Collect().histogram.sample_count != operations ||
        !otel_collected)
    {
        throw std::runtime_error{"histogram delivery validation failed"};
    }

    std::cout << "mtf_0.1.1,histogram," << operations << ',' << mtf_rate << '\n'
              << "prometheus_cpp_1.3.0,histogram," << operations << ','
              << prometheus_rate << '\n'
              << "opentelemetry_cpp_1.9.1,histogram," << operations << ','
              << otel_rate << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::uint64_t operations = argc > 1
            ? std::stoull(argv[1])
            : 1'000'000;
        std::cout << "library,scenario,operations,throughput_per_second\n";
        counter_benchmark(operations);
        histogram_benchmark(operations);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "benchmark failed: " << exception.what() << '\n';
        return 1;
    }
}
