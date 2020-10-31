# Synchronization of multiple clock domains

As per [6756fb05][6756fb05] Perfetto allows to deal with events using different
clock domains. On top of the default set of builtin clock domains, new clock
domains can be dynamically created at trace-time.

Clock domains are allowed to drift from each other.
At import time, Perfetto's [Trace Processor](/docs/trace-processor.md) is able
to rebuild the clock graph and use that to re-synchronize events on a global
trace time, as long as [ClockSnapshot][clock_snapshot] packets are present in
the trace.

Problem statement
-----------------
In a complex multi-producer scenario, different data source can emit events
using different clock domains.

Some examples:

* On Linux/Android, Ftrace events are emitted using the `CLOCK_BOOTTIME` clock,
  but the Android event log uses `CLOCK_REALTIME`.
  Some other data sources can use `CLOCK_MONOTONIC`.
  These clocks can drift over time from each other due to suspend/resume.
* Graphics-related events are typically timestamped by the GPU, which can use a
  hardware clock source that drifts from the system clock.

At trace-time, the data sources might not be able to use `CLOCK_BOOTTIME` (or
even when possible, doing so might be prohibitively expensive).

To solve this, we allow events to be recorded with different clock domains and
re-synchronize them at import time using clock snapshots.

Trace proto syntax
------------------

Clock synchronization is based on two elements of the trace:

### 1. The [`timestamp_clock_id`][timestamp_clock_id] field of TracePacket

```protobuf
message TracePacket {
  optional uint64 timestamp = 8;

  // Specifies the ID of the clock used for the TracePacket |timestamp|. Can be
  // one of the built-in types from ClockSnapshot::BuiltinClocks, or a
  // producer-defined clock id.
  // If unspecified it defaults to BuiltinClocks::BOOTTIME.
  optional uint32 timestamp_clock_id = 58;

```

This (optional) field determines the clock domain for the packet.
If omitted it refers to the default clock domain of the trace
(`CLOCK_BOOTTIME` for Linux/Android).
It present, this field can be set to either:

* One of the [builtin clocks defined in clock_snapshot.proto][builtin_clocks]
  (e.g., `CLOCK_BOOTTIME`, `CLOCK_REALTIME`, `CLOCK_MONOTONIC`). These clocks
  have an ID <= 63.
* A custom sequence-scoped clock, with 64 <= ID < 128
* A custom globally-scoped clock, with 128 <= ID < 2**32

#### Builtin clocks
Builtin clocks cover the most common case of data sources using one of the
POSIX clocks (see `man clock_gettime`). These clocks are periodically
snapshotted by the `traced` service. The producer doesn't need to do anything
else other than setting the `timestamp_clock_id` field in order to emit events
that are use these clocks.

#### Sequence-scoped clocks
Sequence-scoped clocks are application-defined clock domains that are valid only
within the sequence of TracePacket(s) written by the same `TraceWriter`
(i.e. TracePacket that have the same `trusted_packet_sequence_id` field).
In most cases this really means *"events emitted by the same data source on
the same thread"*.

This covers the most common use case of a clock domain that is used only within
a data source and not shared across different data sources.
The main advantage of sequence-scoped clocks is that avoids the ID
disambiguation problem and JustWorks&trade; for the most simple cases.

In order to make use of a custom sequence-scoped clock domain a data source
must:

* Emit its packets with a `timestamp_clock_id` in the range [64, 127]
* Emit at least once a [`ClockSnapshot`][clock_snapshot] packet.

Such `ClockSnapshot`:

* Must be emitted on the same sequence (i.e. by the same `TraceWriter`) that is
  used to emit other `TracePacket`(s) that refer to such `timestamp_clock_id`.
* Must be emitted before the custom clock is referred to by any `TracePacket`
  written by the same `TraceWriter`.
* Must contain a snapshot of: (i) the custom clock id [64, 127] and (ii) another
  clock domain that can be resolved, at import time, against the default trace
  clock domain (`CLOCK_BOOTTIME`) (see the [Operation section](#operation)
  below).

Collisions of `timestamp_clock_id` across two different `TraceWriter` sequences
are okay. E.g., two data sources, unaware of each other, can both use clock ID
64 to refer to two different clock domains.

#### Globally-scoped clocks
Globally-scoped clock domains work similarly to sequence-scoped clock domains,
with the only difference that their scope is global and applies to all
`TracePacket`(s) of the trace.

The same `ClockSnapshot` rules as above apply. The only difference is that once
a `ClockSnapshot` defines a clock domain with ID >= 128, that clock domain can
be referred to by any `TracePacket` written by any `TraceWriter` sequence.

Care must be taken to avoid collisions between global clock domains defined by
different data sources unaware of each other.

As such, it is **strongly discouraged** to just use the ID 128 (or any other
arbitrarily chosen value). Instead the recommended pattern is:

* Chose a fully qualified name for the clock domain
  (e.g. `com.example.my_subsystem`)
* Chose the clock ID as `(HASH("com.example.my_subsystem") + 128) & 0xFFFFFFF`
  where `HASH(x)` is the FNV-1a hash of the fully qualified clock domain name.

### 2. The [`ClockSnapshot`][clock_snapshot] trace packet

The [`ClockSnapshot`][clock_snapshot] packet defines sync points between two or
more clock domains. It conveys the notion *"at this point in time, the timestamp
of the clock domains X,Y,Z was 1000, 2000, 3000."*.

The trace importer ([Trace Processor](/docs/trace-processor.md)) uses this
information to establish a mapping between these clock domain. For instance,
to realize that 1042 on clock domain X == 3042 on clock domain Z.

The `traced` service automatically emits `ClockSnapshot` packets for the builtin
clock domains on a regular basis.

A data source should emit `ClockSnapshot` packets only when using custom clock
domains, either sequence-scoped or globally-scoped.

It is *not* mandatory that the `ClockSnapshot` for a custom clock domain
contains also a snapshot of `CLOCK_BOOTTIME` (although it is advisable to do
so when possible). The Trace Processor can deal with multi-path clock domain
resolution based on graph traversal (see the [Operation](#operation) section).

## Operation

At import time Trace Processor will attempt to convert the timestamp of each
TracePacket down to the trace clock domain (`CLOCK_BOOTTIME`) using the
`ClockSnapshot` packets seen until then using nearest neighbor approximation.

For instance, assume that the trace contains `ClockSnapshot` for
`CLOCK_BOOTTIME` and `CLOCK_MONOTONIC` as follows:

```python
CLOCK_MONOTONIC     1000    1100   1200   1900  ...  2000   2100
CLOCK_BOOTTIME      2000    2100   2200   2900  ...  3500   3600
```

In this example `CLOCK_MONOTONIC` is 1000 ns ahead of `CLOCK_BOOTTIME` until
T=2900. Then the two clocks go out of sync (e.g. the device is suspended) and,
on the next snapshot, the two clocks are 1500 ns apart.

If a `TracePacket` with `timestamp_clock_id=CLOCK_MONOTONIC` and
`timestamp=1104` is seen, the clock sync logic will:

1. Find the latest snapshot for `CLOCK_MONOTONIC` <= 1104 (in the example above
   the 2nd one with `CLOCK_MONOTONIC=1100`)
2. Compute the clock domain conversion to `CLOCK_BOOTTIME` by applying the
   delta (1104 - 1100) to the corresponding `CLOCK_BOOTTIME` snapshot
   (2100, so 2100 + (1104 - 1100) -> 2104).

The example above is rather simple, because the source clock domain  (i.e. the
one specified by the `timestamp_clock_id` field) and the target clock domain
(i.e. the trace time, `CLOCK_BOTTIME`) are snapshotted within the same
`ClockSnapshot` packets.

Clock domain conversion is possible also in more complex scenarios where the
two domains are not directly connected, as long as a path exist between the two.

In this sense `ClockSnapshot` packets define edges of an acyclic graph that is
queried to perform clock domain conversions. All types of clock domains can be
used in the graph search.

In the more general case, the clock domain conversion logic operates as follows:

* The shortest path between the source and target clock domains is identified,
  using a breadth first search in the graph.
* For each clock domain of the path identified, the timestamp is converted using
  the aforementioned nearest neighbor resolution.

This allows to deal with complex scenarios as follows:

```python
CUSTOM_CLOCK        1000                 3000
CLOCK_MONOTONIC     1100       1200      3200          4000
CLOCK_BOOTTIME                 5200                    9000
```

In the example above, there is no snapshot that directly links `CUSTOM_CLOCK`
and `CLOCK_BOOTTIME`. However there is an indirect path that allows a conversion
via `CUSTOM_CLOCK -> CLOCK_MONOTONIC -> CLOCK_BOOTTIME`.

This allows to synchronize a hypothetical `TracePacket` that has
`timestamp_clock_id=CUSTOM_CLOCK` and `timestamp=3503` as follows:

```python
#Step 1
CUSTOM_CLOCK = 3503
Nearest snapshot: {CUSTOM_CLOCK:3000, CLOCK_MONOTONIC:3200}
CLOCK_MONOTONIC = (3503 - 3000) + 3200 = 3703

#Step 2
CLOCK_MONOTONIC = 3703
Nearest snapshot: {CLOCK_MONOTONIC:1200, CLOCK_BOOTTIME:5200}
CLOCK_BOOTTIME = (3703 - 1200) + 5200 = 7703
```

Caveats
-------
Clock resolution between two domains (A,B) is allowed only as long as all the
clock domains in the A -> B path are monotonic (or at least look so in the
`ClockSnapshot` packets).
If non-monotonicity is detected at import time, the clock domain is excluded as
a source path in the graph search and is allowed only as a target path.

For instance, imagine capturing a trace that has both `CLOCK_BOOTTIME`
and `CLOCK_REALTIME` in the night when daylight saving is applied, when the
real-time clock jumps back from 3AM to 2AM.

Such a trace would contain several snapshots that break bijectivity between the
two clock domains. In this case converting a `CLOCK_BOOTTIME` timestamp to
`CLOCK_REALTIME` is always possible without ambiguities (eventually two distinct
timestamps can be resolved against the same `CLOCK_REALTIME` timestamp).
The opposite is not allowed, because `CLOCK_REALTIME` timestamps between 2AM
and 3AM are ambiguous and could be resolved against two different
`CLOCK_BOOTTIME` timestamps).

[6756fb05]: https://android-review.googlesource.com/c/platform/external/perfetto/+/1101915/
[clock_snapshot]: https://android.googlesource.com/platform/external/perfetto/+/refs/heads/master/protos/perfetto/trace/clock_snapshot.proto
[timestamp_clock_id]: https://android.googlesource.com/platform/external/perfetto/+/3e7ca4f5893f7d762ec24a2eac9a47343b226c6c/protos/perfetto/trace/trace_packet.proto#68
[builtin_clocks]: https://android.googlesource.com/platform/external/perfetto/+/3e7ca4f5893f7d762ec24a2eac9a47343b226c6c/protos/perfetto/trace/clock_snapshot.proto#25
