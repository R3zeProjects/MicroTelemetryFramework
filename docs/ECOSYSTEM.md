# VOSP ecosystem assessment

## Current composition

| Framework | Benchmarked release | Responsibility |
| --- | --- | --- |
| MCF | 0.5.0-beta | compile-time structural contracts |
| MEF | 0.6.0-beta | errors, logging, bounded workers |
| MPF | 0.2.0-beta | persistence values, codecs, stores and MEF sink |
| MTF | 0.1.1-beta | metrics, events, spans and exporter pipelines |

Dependencies point toward MCF or are composed in the application. MCF has no
runtime state. The ecosystem benchmark pins exact commits and places the only
MTF-to-MPF exporter implementation in the composition root; neither framework
depends on the other.

## What the ecosystem can build today

- classify and route typed failures;
- log them synchronously or asynchronously;
- persist structured log records;
- count operations and observe latency distributions;
- emit events and completed spans;
- export telemetry synchronously or through a bounded queue;
- apply backpressure and perform drain-first shutdown;
- replace structural error and exporter implementations at compile time.

The benchmarked transaction performs one MEF log operation, one MTF counter
update, one MTF event export, and persists both produced records through an
MPF-compatible journal. MCF validates the boundaries at compile time.

## Maturity assessment

| Area | Assessment | Evidence / gap |
| --- | --- | --- |
| Architecture | strong beta | directional dependencies, owning values, bounded state |
| API clarity | strong beta | canonical `<vosp/...>` headers and policy-selected modes |
| Performance evidence | strong beta | pinned external and full-contour benchmarks |
| Safety validation | strong beta | GCC/Clang/MSVC, sanitizers, TSan, clang-tidy, package consumers |
| Interoperability | developing | structural exporters exist; standard wire exporters are not shipped |
| Operational maturity | early | no long-running production deployment or incident history yet |
| Compatibility | beta | all components remain pre-1.0 and do not promise stable ABI |

Overall, the ecosystem is suitable for portfolio demonstration, prototypes,
internal tools, controlled services, and pilot integrations. It is not yet an
OpenTelemetry replacement or a universally production-proven platform.

## Main gaps before 1.0

1. one real service integration with sustained load and failure injection;
2. stable cross-framework version compatibility policy;
3. OpenTelemetry/Prometheus wire exporters and trace-context propagation;
4. persistence crash-recovery and durability guarantees;
5. longer Linux soak, packaging on multiple distributions, and release signing;
6. user feedback from an integration not authored inside this ecosystem.

The engineering foundation is credible; the remaining work is primarily
interoperability and operational evidence rather than another core rewrite.
