# Oilpan: C++ Garbage Collection

Oilpan is an open-source garbage collection library for C++ that can be used stand-alone or in collaboration with V8's JavaScript garbage collector.

**Key properties**
- Trace-based garbage collection;
- Precise on-heap memory layout;
- Conservative on-stack memory layout;
- Allows for collection with and without considering stack;
- Incremental and concurrent marking;
- Incremental and concurrent sweeping;
- Non-incremental and non-concurrent compaction for selected spaces;

See the [Hello World](https://chromium.googlesource.com/v8/v8/+/main/samples/cppgc/hello-world.cc) example on how to get started using Oilpan to manage C++ code.

Oilpan follows V8's project organization, see e.g. on how we accept [contributions](https://v8.dev/docs/contribute) and [provide a stable API](https://v8.dev/docs/api).
