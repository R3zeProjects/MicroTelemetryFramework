#include <vosp/telemetry/pipeline.hpp>

static_assert(vosp::contracts::TelemetryExporter<
              vosp::telemetry::IExporter,
              vosp::telemetry::Record>);
