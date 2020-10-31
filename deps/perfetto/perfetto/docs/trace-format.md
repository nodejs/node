# Perfetto trace format

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): write trace format doc. -->
***

A Perfetto trace is guaranteed to be a a linear sequence of `TracePacket(s)`
(see [trace_packet.proto](/protos/perfetto/trace/trace_packet.proto)).

As a key part of the Perfetto design, the tracing service is agnostic of the
content of TracePacket, modulo few fields (e.g., `trusted_packed_*`,
clock snapshots, copy of the original config) that are produced by the service
itself.

Each data source can extend the trace with their app-specific protobuf schema.
*** aside
TODO(primiano): we should reserve an extension range and figure out / comment a
hash to assign sub-message IDs, even without checking them into
trace_packet.proto.
***


**Linearity guarantees**  
The tracing service guarantees that all `TracePacket(s)` written by a given
`TraceWriter` are seen in-order, without gaps or duplicates. If, for any reason,
a `TraceWriter` sequence becomes invalid, no more packets are returned to the
Consumer (or written into the trace file).

However, `TracePacket(s)` written by different `TraceWriter` (hence even
different producers) can be seen in no particular order.
The consumer can re-establish a total order, if interested, using the packet
timestamps (after having synchronized the different clocks onto a global clock).
