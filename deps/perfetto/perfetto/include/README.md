# Perfetto public API surface

**This API surface is not stable yet, don't depend on it**

This folder contains the public perfetto API headers. This allows an app to
inject trace events into perfetto with ~10 lines of code (see
api_usage_example.cc).

The ext/ subdirectory expose the API-unstable classes and types that are
exposed to emvbedders that have exceptional requirements in terms of interposing
their own custom IPC layer. To the day the only case is chromium. Nothing else
should depend on ext/. Contact perfetto-dev@ if you think you need to 
depend on an ext/ header.

Headers in this folder must be hermetic. No ext/ perfetto header must be
leaked from the includes.

What is a client supposed to do to use tracing? See example below in this page.


Source code layout: what goes where?
------------------------------------

**include/perfetto (this folder):**
Embedders are allowed to access and depend on any folder of this but ext/.
This contains classes to: (i) use tracing; (ii) extend the tracing internals
(i.e. implement the Platform).

Rules:
- This directory should contain only .h files and no .cc files.
- Corresponding .cc files go into `src/`.
- .h files in here can depend only on `include/perfetto/` but not on
  `include/perfetto/ext/`,

**include/perfetto/tracing/internal:**
This directory contains headers that are required to implement the public-facing
tracing API efficiently but that are not part of the API surface.
In an ideal world there would be no need of these headers and everything would
be handle via forward-declarations and PIMPL patterns. Unfortunately, however,
PIMPL cannot be used for inline functions, where the implementation needs to be
exposed in the public headers, which in turn need to depend on the memory layout
of structs/classes.

Rules:
- All classes / types declared in this folder must be wrapped in the
  ::perfetto::internal namespace.
- Both public and internal .h headers must not pull other perfetto headers
  from ext/.
- .cc files instead can depend on other perfetto classes, as well as .h headers
  located in src/.
- Embedders must not depend on the perfetto::internal namespace.
- Internal types cannot be used as input, output or return arguments of public
  API functions.
- Internal types cannot be directly exposed to virtual methods that are
  intended to be called or overridden by the embedder (e.g. TracingBackend's
  methods). For those the solution is to create a matching non-internal base
  class with a static factory method.
- We don't guarantee binary compatibility between versions (i.e. this client
  library can only be statically linked) but we guarantee source-level
  compatibility and ABI of the UNIX socket and shared memory buffers.


Usage example
-------------
1. Call `perfetto::Tracing::Initialize(...)` once, when starting the app.
  While doing so the app can chose the tracing model:
  - Fully in-process: the service runs in a thread within the same process.
  - System: connects to the traced system daemon via a UNIX socket. This allows
    the app to join system-wide tracing sessions. This is available only on
    Linux/Android/MacOS for now.
  - Private dedicated process: similar to the in-process case, but the service
    runs in a dedicated process rather than a thread. This is for performance,
    stability and security isolation. Also, this is not implemented yet.
  - Custom backend: this is for peculiar cases (mainly chromium) where the
    embedder is multi-process but wants to use a different IPC mechanism. The
    embedder needs to deal with the larger and clunkier set of perfetto APIs.
    Reach out to the team before using this mode. It's very unlikely you need
    this unless you are a project rolled into chromium.

2. Define and register one or more data sources, like this:
```cpp
  #include "perfetto/tracing.h"

  class MyDataSource : public perfetto::DataSource<MyDataSource> {
    void OnSetup(const SetupArgs&) override {}
    void OnStart(const StartArgs&) override {}
    void OnStop(const StopArgs&) override {}
  };
  ...
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("my_data_source");
  MyDataSource::Register(dsd);
```

3. Optionally define a new proto schema in `trace_packet.proto`

4. Emit trace events
```cpp
  MyDataSource::Trace([](MyDataSource::TraceContext ctx) {
      auto trace_packet = ctx.NewTracePacket();
      trace_packet->set_timestamp(...);
      auto* my_custom_proto = trace_packet->set_my_custom_proto();
  });
```

The passed labmda will be called only if tracing is enabled and the data source
was enabled in the trace config. It might be called multiple times, one for each
active tracing session, in case of concurrent tracing sessions (or even within a
single tracing session, if the data source is listed twice in the trace config).
