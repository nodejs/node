# Perfetto IPC

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): write IPC doc. -->
***

**TL;DR**  
We needed an IPC for Android and Linux which was small, simple, controllable,
predictable, C++11 friendly and debuggable.
Our IPC transport is not mandatory outside of Android, you can wrap your own IPC
transport (e.g., Perfetto uses Mojo in chromium) or just short circuit the
Perfetto `{Service,Producer,Consumer}` interfaces for IPC-less full in-process
use.

Key features:
- Protobuf over a unix-socket.
- Allows to send file descriptors over the wire: for setting up shared memory
  and passing the FD for the output trace from a consumer to the service.
- Service definition uses same protobuf rpc syntax of [gRPC](https://grpc.io)
- Extremely simple [wire protocol](/protos/perfetto/ipc/wire_protocol.proto).
- C++11 friendly, allows to bind `std::function` to each request.
- Leak (un)friendly: tries hard to guarantee that callbacks are left unresolved,
  using C++11 move semantics.
- Shutdown / destruction friendly: tries hard to guarantee that no callbacks are
  issued after the IPC channel is destroyed.
- Disconnection-friendly: all outstanding requests (and hence pending callbacks)
  are nack-ed in case of a disconnection (e.g., if the endpoint crashes).
- Memory friendly: one virtually contiguous cache-friendly rx buffer,
  madvise()'d when when not used.
- Debugging friendly: single-thread only, based on non-blocking socket I/O.
- Binary size friendly: generates one protobuf per message, doesn't have any
  external dependency.
- Safe:
  - The rx buffer has guard regions around.
  - The wire protocol is based on protobuf.
  - [Fuzzed](/src/ipc/buffered_frame_deserializer_fuzzer.cc)
- Offers direct control of socket buffers and overrun/stalling policy.
- ABI stable.

Realistically will never support:
  - Multithreading / thread pools.
  - Congestion or flow control.
  - Non-data object brokering (e.g. sending a remote interface).
  - Introspection / reflection.
