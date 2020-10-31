# Life of a Perfetto tracing session

This document explains how producer, service and consumer interact end-to-end
during a tracing session, with references to code and IPC requests / responses.

1.  One or more producers connect to the tracing service and sets up their IPC
    channels.
2.  Each producer advertises one or more data sources through the
    [`RegisterDataSource`](/protos/perfetto/ipc/producer_port.proto#34) IPC.
    Nothing more happens on the Producer until this point. Nothing traces by
    default.
3.  A consumer connects to the tracing service and sets up the IPC channel.
4.  The consumer starts a tracing session sending a
    [trace config](trace-config.md) to the service through the
    [`EnableTracing`](/protos/perfetto/ipc/consumer_port.proto#65) IPC.
6.  The service creates as many new trace buffers as specified in the config.
7.  The service iterates through the
    [`data_sources`](/protos/perfetto/config/trace_config.proto#50) section of
    the trace config: for each entry, if a matching data source is found in the
    producer(s) (accordingly to what advertised in step 2):
8.  The service sends a
    [`SetupTracing`](/protos/perfetto/ipc/producer_port.proto#112) IPC message,
    passing a shared memory buffer  to the producer(s) (only once for each
    producer).
9.  The service sends a
    [`StartDataSource`](/protos/perfetto/ipc/producer_port.proto#105) IPC message
    to each producer, for each data source configured in the trace config and
    present in the producer (if any).
10. The producer creates one or more data source instance, as instructed in
    the previous step.
11. Each data source instance creates one or more
    [`TraceWriter`](/include/perfetto/ext/tracing/core/trace_writer.h) (typically
    one for each thread).
12. Each `TraceWriter` writes one or more
    [`TracePacket`](/protos/perfetto/trace/trace_packet.proto).
13. While doing so, each `TraceWriter` takes ownership of shared memory buffer's
    chunks, using the [`SharedMemoryArbiter`](/include/perfetto/ext/tracing/core/shared_memory_arbiter.h).
14. While writing a `TracePacket`, the `TraceWriter` will unavoidably cross the
    chunk boundary (typically 4KB, but can configured to be smaller).
15. When this happens the `TraceWriter` will release the current chunk and
    acquire a new chunk through the `SharedMemoryArbiter`.
16. The `SharedMemoryArbiter` will, out-of-band, send a
    [`CommitDataRequest`](/protos/perfetto/ipc/producer_port.proto#41) to the
    service, requesting it to move some chunks of the shared memory buffer into
    the final trace buffer.
17. If one or more long `TracePacket` were fragmented over several chunks, it is
    possible that some of these chunks are gone from the shared memory
    buffer and committed into the final trace buffer (step 16). In this case,
    the `SharedMemoryArbiter` will send an other `CommitDataRequest` IPC message
    to request the out-of-band patching of the chunk data into the final trace
    buffer.
18. The service will check if the given chunk, identified by the tuple
    `{ProducerID (unspoofable), WriterID, ChunkID}` is still present in the
    trace buffer and if so will proceed to patch it (% sanity checks).
19. The consumer sends a [`FlushRequest`](/protos/perfetto/ipc/consumer_port.proto#52)
    to the service, asking it commit all data on flight in the trace buffers.
20. The service, in turn, issues a
    [`Flush`](/protos/perfetto/ipc/producer_port.proto#132) request to all
    producers involved in the trace session.
21. The producer(s) will `Flush()` all their `TraceWriter` and reply to the
    service flush request.
22. Once the service has received an ACK to all flush requests from all
    producers (or the
    [flush timeout](/protos/perfetto/ipc/consumer_port.proto#117) has expired)
    it replies to the consumer's `FlushRequest`
23. The consumer optionally sends a
    [`DisableTracing`](/protos/perfetto/ipc/consumer_port.proto#38) IPC request
    to stop tracing and freeze the content of the trace buffers.
24. The service will in turn broadcast a
    [`StopDataSource`](/protos/perfetto/ipc/producer_port.proto#110) request for
    each data source in each Producer.
23. At this point the Consumer issues a
    [`ReadBuffers`](/protos/perfetto/ipc/consumer_port.proto#41) IPC request
    (unless it did previously pass a file descriptor to the service when
    enabling the trace through the `write_into_file` field of the trace config).
24. The service reads the trace buffers and streams all the `TracePacket(s)`
    back to the consumer.
25. If a trace packet stored in the trace buffer is incomplete (e.g., a fragment
    is missing) or is marked as pending out-of-band patching, the given writer
    sequence is interrupted and no more packets for that sequence are read.
    Other packets for other `TraceWriter` sequences will be unaffected.
26. The consumer sends a `FreeBuffers` (or simply disconnects).
27. The service tears down all the trace buffers for the session.
