# Changelog

## Unreleased

- Updated MCF, MEF, and MPF integration pins to the validated ecosystem baseline.
- Removed the temporary `std::string` allocation when registry entries are erased
  through a `std::string_view`.

## 0.1.1-beta - 2026-08-22

- Added pinned counter and histogram comparisons with prometheus-cpp 1.3.0
  and OpenTelemetry C++ 1.9.1.
- Added a validated MCF + MEF + MPF + MTF end-to-end benchmark.
- Documented benchmark equivalence limits and ecosystem maturity.

## 0.1.0-beta - 2026-08-22

- Added owning metrics, events, spans, and attributes.
- Added atomic counters/gauges and fixed-boundary histograms.
- Added a bounded thread-safe instrument registry.
- Added direct and bounded asynchronous exporter pipelines.
- Added batching, blocking backpressure, flush, drain shutdown, and statistics.
- Added MCF telemetry contract validation, CMake packaging, tests, benchmarks,
  examples, sanitizers, static analysis, and cross-platform CI.
