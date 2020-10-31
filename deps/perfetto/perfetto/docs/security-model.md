# Perfetto security model

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): expand security model doc. -->
***

![Security overview](https://storage.googleapis.com/perfetto/markdown_img/security-overview.png)

**TL;DR**  
The tracing service has two endpoints (in Chromium: Mojo services, on Android:
UNIX sockets): one for producer(s) and one for consumer(s).
The former is typically public, the latter is restricted only to trusted
consumers.

**Producers**  
Producers are never trusted. We assume they will try their best to DoS / crash /
exploit the tracing service. We do so at the
[core/tracing_service_impl.cc](/src/tracing/core/tracing_service_impl.cc) so
that the same level of security and testing is applied regardless of the
embedder and the IPC transport.

**Tracing service**  
- The tracing service has to validate all inputs.
- In the worst case a bug in the tracing service allowing remote code execution,
  the tracing service should have no meaningful capabilities to exploit.
- The tracing service, by design, has a limited syscall surface to simplify
  its sandboxing:
  - It doesn't open or create files (% tmpfs).
  - It writes only onto file descriptors passed over the IPC channel.
  - It doesn't open or create sockets (on Android the IPC sockets are passed by
    init, see [perfetto.rc](/perfetto.rc))
  - On Android it runs as nobody:nobody and is allowed to do very little
    see [traced.te](https://android.googlesource.com/platform/system/sepolicy/+/master/private/traced.te).
  - In Chromium it should run as a utility process.
  - TODO: we could use BPF syscall sandboxing both in Chromium and Android.
    [Proof of concept](https://android-review.googlesource.com/c/platform/external/perfetto/+/576563)

**Consumers**  
Consumers are always trusted. They still shouldn't be able to crash or exploit
the service. They can easily DoS it though, but that is WAI.
  - In Chromium the trust path is established through service manifest.
  - In Android the trust path is established locking down the consumer socket
    to shell through SELinux.

**Shared memory isolation**  
Memory is shared only point-to-point between each producer and the tracing
service. We should never ever share memory across producers (in order to not
leak trace data belonging to different producers) nor between producers and
consumers (that would open a hard to audit path between
untrusted-and-unprivileged and trusted-and-more-privileged entities).

**Attestation of trace contents**  
The tracing service guarantees that the `TracePacket` fields written by the
Service cannot be spoofed by the Producer(s).  
Packets that try to define those fields are rejected, modulo clock snapshots.  
See [PacketStreamValidator](/src/tracing/core/packet_stream_validator.cc) and
[its unit test](/src/tracing/core/packet_stream_validator_unittest.cc) for more
details.  
At the moment nothing prevents that a producer writes `TracePacket(s)` that do
not belong to its data sources. Realistically the service will never prevent
that because doing so would imply that the service knows about all the possible
types of packets, which doesn't scale.  
However, the service appends the POSIX uid of the producer to each `TracePacket`
to perform offline attestation of the contents of the trace.
