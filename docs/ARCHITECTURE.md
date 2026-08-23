# MicroTelemetryFramework architecture

## Scope

MTF owns in-process telemetry values, metric aggregation, registry cardinality,
and exporter scheduling. It deliberately does not own network protocols,
persistent storage, error logging, service discovery, or backend-specific
schemas.

## Operating principles

Instrumentation and export are intentionally separate. Instruments update
their owned aggregation state; snapshots turn that state into observations.
Events and spans create owning `Record` envelopes immediately, so exporters may
retain records after the producer call returns.

```text
instrument update -> atomic/mutex-protected state -> snapshot -> Record
event/span -----------------------------------------------^
Record -> direct pipeline -> exporter on producer thread
Record -> async pipeline  -> bounded queue -> worker -> exporter
```

The direct pipeline exposes exporter latency to the producer. The asynchronous
pipeline moves accepted records into a fixed-capacity queue, applies
backpressure at capacity, exports bounded batches, rejects publication after
shutdown begins, and drains already accepted records before joining its worker.
Exporter callbacks run without the queue lock and the pipeline retains shared
ownership of the exporter.

Metric synchronization matches the required invariant: counters and gauges use
relaxed atomics for independent values, while histogram snapshots use one mutex
for internally consistent buckets, count, and sum. Registry and queue limits
bound memory growth. Backend protocols, retries, persistence, and global
registries remain explicit application-level composition choices.

## Dependency direction

```text
MicroContractsFramework (concepts only)
                 ^
                 |
MicroTelemetryFramework (runtime implementation)
                 ^
                 |
 application + chosen exporters
```

MTF depends only on MCF and the C++23 standard library. Exporters may compose
MEF, MPF, OpenTelemetry, Prometheus, a file writer, or application code at the
composition root. Those integrations must not become dependencies of MTF core.

## Components

### Records

`Record` is an owning envelope containing a name, wall-clock timestamp,
attributes, and a `MetricData`, `EventData`, or `SpanData` payload. Exporters can
retain a record after a callback returns. `Span` measures duration with a
monotonic clock while preserving a wall-clock start timestamp.

### Instruments and registry

`Counter` and `Gauge` use relaxed atomics because they provide independent
numeric observations rather than ordering application memory. `Histogram`
protects buckets, count, and sum with one mutex so each snapshot is internally
consistent.

`Registry` owns instrument states. Returned handles share those states and
remain valid after registry removal. Names are unique across instrument kinds.
The default cardinality limit is 4096 and the hard supported limit is 1,000,000.

### Export pipelines

The direct pipeline calls its exporter on the producer thread. The asynchronous
pipeline owns a fixed-capacity queue and one worker. Producers block at
capacity, the worker exports bounded batches, and shutdown rejects new records
before draining accepted records. Export callbacks execute without holding the
queue mutex.

The pipeline shares ownership of its exporter. This prevents an exporter from
being destroyed while an asynchronous callback is in flight.

## Non-goals for 0.1

- backend-specific wire formats;
- distributed trace context propagation;
- periodic collection scheduler;
- dynamic histogram boundaries;
- process-global registries;
- implicit retries or unbounded queues.

These features can be added at explicit extension seams without changing the
instrument storage model.
