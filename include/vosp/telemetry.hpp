#pragma once

/** @file telemetry.hpp Public umbrella for MicroTelemetryFramework. */

#include <vosp/telemetry/exporter.hpp>
#include <vosp/telemetry/instruments.hpp>
#include <vosp/telemetry/pipeline.hpp>
#include <vosp/telemetry/record.hpp>
#include <vosp/telemetry/registry.hpp>
#include <vosp/telemetry/version.hpp>

namespace vosp
{
/** @brief Compact default synchronous telemetry pipeline. */
using Telemetry = telemetry::Pipeline<telemetry::pipeline_policy::Direct>;

/** @brief Policy-selected telemetry pipeline. */
template<typename Policy>
using TelemetryPipeline = telemetry::Pipeline<Policy>;
} // namespace vosp

#ifndef VOSP_NAMESPACE_FACADE_DEFINED
#define VOSP_NAMESPACE_FACADE_DEFINED
/** @brief Compact namespace facade shared by the VOSP ecosystem. */
namespace vsp = vosp;
#endif
