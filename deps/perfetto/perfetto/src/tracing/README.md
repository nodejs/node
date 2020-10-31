How to use this library
-----------------------
There are three options to use this library:

## Option 1) Fully in-process
In this mode Producer, Consumers and the Service are hosted in the same process.
This is not too interesting other than tests and particular cases of nesting
tracing instances coming from different libraries within the same process
(concrete example v8, skia and webrtc in Chrome).
In this configuration, the client is expected to at least:
- Create a TracingService instance via TracingService::CreateInstance
  (see `core/tracing_service.h`)
- Subclass Producer (`core/producer.h`) and connect it to the service.
- Provide a TaskRunner implementation (see `test/test_task_runner.h`)
- Provide a trivial SharedMemory implementation (`core/shared_memory.h`) which
  is simply backed by a malloc() buffer.

## Option 2) Using the provided UNIX RPC transport
The `include/unix_rpc` provides the building blocks necessary to implement a RPC
mechanism that allows Producer(s), Consumer(s) and Service to be hosted on
different processes on the same machine and talk over a UNIX domain socket.
- Producer(s) are expected to get a service proxy via
`UnixServiceConnection::ConnectAsProducer()`.
- The `Service` must be instantiated via `UnixServiceHost::CreateInstance()`. The
returned instance encapsulates the `Service` and exposes two UNIX sockets (one
for Producer(s), one for Consumer(s)) on the current process.

## Option 3) Providing a custom RPC transport
Similar to Option 2, but the client creates its own transport mechanism,
defining how methods are proxies between instances and providing a SharedMemory
implementation that can be transferred through RPC. Concrete example of this is
Chrome implementing this library over a Mojo transport.


Directory layout
----------------

`include/`
Is the public API that clients of this library are allowed to depend on.
Headers inside include/ cannot depend on anything else.

`src/`
Is the actual implementation that clients can link but not expected to access
at a source-code level.


**Both have the following sub-structure**:

`{include,src}/core/`
"Core" is the pure c++11 tracing machinery that deals with bookkeeping,
ring-buffering, partitioning and multiplexing but knows nothing about
platform-specific things like implementation of shared memory and RPC mechanism.

`{include,src}/unix_rpc/`
A concrete implementation of the transport layer based on unix domain sockets
and posix shared memory.
