# Benchmark methodology

`MicroTelemetryFrameworkBenchmark` is an opt-in repository target. It is never
installed or exported by the CMake package.

The initial baseline executes two one-million-operation scenarios:

1. relaxed atomic increments through a copied `Counter` handle;
2. construction and publication of owning event records through an async
   pipeline configured with capacity 1024 and batch size 64.

The async exporter performs one relaxed atomic addition per batch and verifies
that all records were delivered. Timing includes queue contention,
backpressure, record moves, worker wakeups, and exporter invocation. It excludes
process startup and pipeline construction.

Results in the README are the median of five consecutive Release runs. Compare
versions only with the same compiler, machine, power plan, iteration count, and
benchmark source. Debug numbers and results from different machines must not be
presented as regressions or improvements.
