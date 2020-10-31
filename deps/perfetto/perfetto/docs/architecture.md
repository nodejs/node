# Perfetto key concepts and architecture

Producer <> Service <> Consumer model
-------------------------------------
![Perfetto Stack](https://storage.googleapis.com/perfetto/markdown_img/producer-service-consumer.png)

**Service**  
The tracing service is a long-lived entity (a system daemon on Linux/Android,
a service in Chrome) that has the following responsibilities:
- Maintains a registry of active producers and their data sources.
- Owns the trace buffers.
- Handles multiplexing of several tracing sessions.
- Routes the trace config from the consumers to the corresponding producers.
- Tells the Producers when and what to trace.
- Moves data from the Producer's shared memory buffer to the central non-shared
  trace buffers.

**Producer**  
A producer is an untrusted entity that offers the ability to contribute to the
trace. In a multiprocess model, a producer almost always corresponds to a client
process of the tracing service. It advertises its ability to contribute to the trace with one or more data sources.
Each producer has exactly:
- One shared memory buffer, shared exclusively with the tracing service.
- One IPC channel with the tracing service.

A producer is completely decoupled (both technically and conceptually) from
consumer(s). A producer knows nothing about:
- How many consumer(s) are connected to the service.
- How many tracing sessions are active.
- How many other producer(s) are registered or active.
- Trace data written by other producer(s).

*** aside
In rare circumstances a process can host more than one producer and hence more
than one shared memory buffer. This can be the case for a process bundling
third-party libraries that in turn include the Perfetto client library.  
Concrete example: at some point in the future Chrome might expose one Producer for tracing within the main project, one for V8 and one for Skia (for each child
process).
***

**Consumer**  
A consumer is a trusted entity (a cmdline client on Linux/Android, an interface
of the Browser process in Chrome) that controls (non-exclusively) the tracing service and reads back (destructively) the trace buffers.
A consumer has the ability to:
- Send a [trace config](trace-config.md) to the service, determining:
 - How many trace buffers to create.
 - How big the trace buffers should be.
 - The policy for each buffer (*ring-buffer* or *stop-when-full*).
 - Which data sources to enable.
 - The configuration for each data source.
 - The target buffer for the data produced by each data source configured.
- Enable and disable tracing.
- Read back the trace buffers:
  - Streaming data over the IPC channel.
  - Passing a file descriptor to the service and instructing it to periodically
    save the trace buffers into the file.

**Data source**  
A data source is a capability, exposed by a Producer, of providing some tracing
data. A data source almost always defines its own schema (a protobuf) consisting
of:
- At most one `DataSourceConfig` sub-message
  ([example](/protos/perfetto/config/ftrace/ftrace_config.proto))
- One or more `TracePacket` sub-messages
  ([example](/protos/perfetto/trace/ps/process_tree.proto))

Different producers may expose the same data source. Concrete example:
*** aside
At some point in the near future we might offer, as part of Perfetto, a library
for in-process heap profiling. In such case more than one producer, linking
against the updated Perfetto library, will expose the heap profiler data source,
for its own process.
***

**IPC channel**  
In a multiprocess scenario, each producer and each consumer interact with the
service using an IPC channel. IPC is used only in non-fast-path interactions,
mostly handshakes such as enabling/disabling trace (consumer), (un)registering
and starting/stopping data sources (producer). The IPC is typically NOT employed
to transport the protobufs for the trace.
Perfetto provides a POSIX-friendly IPC implementation, based on protobufs over a
UNIX socket (see [ipc.md](ipc.md)). That IPC implementation is not mandated.
Perfetto allows the embedder:
- Wrap its own IPC subsystem (e.g., Perfetto in Chromium uses Mojo)
- Not use an IPC mechanism at all and just short circuit the
  Producer <> Service <> Consumer interaction via `PostTask(s)`.

See [embedder-guide.md](embedder-guide.md) for more details.


**Shared memory buffer**  
Producer(s) write tracing data, in the form of protobuf-encoded binary blobs,
directly into its shared memory buffer, using a special library called
[ProtoZero](protozero.md). The shared memory buffer:
- Has a fixed and typically small size (configurable, default: 128 KB).
- Is an ABI and must maintain backwards compatibility.
- Is shared by all data sources of the producer.
- Is independent of the number and the size of the trace buffers.
- Is independent of the number of Consumer(s).
- Is partitioned in *chunks* of variable size.

Each chunk:
- Is owned exclusively by one Producer thread (or shared through a mutex).
- Contains a linear sequence of [`TracePacket(s)`](trace-format.md), or
  fragments of that. A `TracePacket` can span across several chunks, the
  fragmentation is not exposed to the consumers (consumers always see whole
  packets as if they were never fragmented).
- Can be owned and written by exactly one `TraceWriter`.
- Is part of a reliable and ordered sequence, identified by the `WriterID`:
  packets in a sequence are guaranteed to be read back in order, without gaps
  and without repetitions (see [trace-format.md](trace-format.md) for more).

See the comments in
[shared_memory_abi.h](/include/perfetto/ext/tracing/core/shared_memory_abi.h)
for more details about the binary format of this buffer.

Other resources
---------------
* [Life of a tracing session](life-of-a-tracing-session.md)
* [Trace config](trace-config.md)
* [Trace format](trace-format.md)
