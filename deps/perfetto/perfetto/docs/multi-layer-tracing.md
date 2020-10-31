# Multi layer tracing

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): write multi-layer tracing doc. -->
***

This doc should explain how is possible to compose a hierarchy of tracing
services. The concrete use case is combining multiprocess tracing in Chromium
with Android's tracing daemons (think to hypervisors' nested page tables).

The TL;DR of the trick is:
- ABI stability of the
  [shared_memory_abi.h](/include/perfetto/ext/tracing/core/shared_memory_abi.h)
- ABI stability of the IPC surface.

The tracing service in chromium should proxy Producer connections (adapting Mojo
to our IPC) towards the Android's `traced` service, passing back the shared
memory buffers to the real producers (the Chrome child process).

Conceptually it is simple and straightforward, requires *some* care to implement
correctly ownership of the shared memory buffers though.
