# Public API and concurrency contracts

## Compatibility

The public API is every declaration reachable from `<vosp/telemetry.hpp>`.
Before 1.0, minor releases may change source compatibility when recorded in the
changelog. Patch releases preserve documented source behavior. No stable ABI is
promised before 1.0; consumers must rebuild this header-only library.

## Ownership

- `Record` and `Attribute` own all strings they expose.
- instrument handles share ownership of their state;
- `Registry` owns registered state but removing a name does not invalidate an
  already returned handle;
- every `Pipeline` shares ownership of its `IExporter`;
- an async queue owns every accepted record until export completes.

## Validation

- instrument names must be non-empty and unique across kinds;
- registry capacity must be in `[1, 1,000,000]`;
- counters reject negative and non-finite deltas;
- gauges reject non-finite values;
- histograms reject non-finite observations and require finite, strictly
  increasing boundaries;
- a span produces at most one completed record;
- null exporters are rejected during pipeline construction.

Invalid instrument definitions throw `std::invalid_argument` or
`std::logic_error`. Hot-path numeric updates return `bool` and do not throw.

## Thread-safety matrix

| Type | Concurrent operations | Contract |
| --- | --- | --- |
| `Record` | const reads | safe after publication |
| `Counter` | `add`, `value` | atomic and thread-safe |
| `Gauge` | `set`, `add`, `value` | atomic and thread-safe |
| `Histogram` | `observe`, registry snapshot | serialized and consistent |
| `Registry` | create, collect, remove, clear, size | internally synchronized |
| `MemoryExporter` | export, snapshot, size, clear | internally synchronized |
| direct `Pipeline` | publish and stats | exporter must accept concurrent callbacks |
| async `Pipeline` | publish, collect, flush, shutdown, stats | internally synchronized; one exporter callback at a time |

Async `flush()` returns `false` when invoked recursively from its own exporter
worker because waiting for that callback would deadlock. `shutdown()` is
idempotent, rejects new work, and drains already accepted records. When an
exporter callback initiates shutdown, the worker cannot join itself; it releases
the thread handle and finishes draining through shared internal state. An
external `flush()` may be used to wait for that completion. Destruction
concurrent with calls through the same pipeline object remains unsupported;
the owner must end producer access before destroying it.

## Exporter contract

An exporter implements:

```cpp
bool export_batch(std::span<const vosp::telemetry::Record> records);
```

Returning `true` acknowledges the complete batch. Returning `false` or throwing
marks the complete batch failed; MTF does not retry implicitly. Exporters must
not retain the span itself, but may copy records from it.
