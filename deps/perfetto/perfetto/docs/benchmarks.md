# Perfetto benchmarks


*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): Summarize results of perfetto_benchmarks  -->
***

This doc should show the charts of `perfetto_benchmarks`, showing cpu usage and
tracing bandwidth for both writing (producer->service) and reading
(service->file / service->consumer).

In two modes:
- Measure peak tracing bandwidth saturating the cpu: the producer(s) write as
  much data as they can, reaching 100% cpu usage.
- Measure CPU overhead vs constant bandwidth: the producer(s) writes data at a
  pre-defined rate.

Tweaking the various parameters, such as:
- Number of writers
- Size of the shared memory buffer
- Size of each TracePacket.

**TL;DR:**  
Peak producer-to-service tracing bandwidth:
* Linux desktop: ~1.3 GB/s
* Android Pixel: ~1 GB/s

Producer-to-service CPU overhead when writing ~3 MB/s: 0.01 - 0.03
(0.01 := 1% cpu time of one core)

CPU overhead for translating ftrace raw pipe into protobuf:
* Android Pixel: 0.00-0.01 when idle.
* Android Pixel: 0.02-0.04 with 8 cores @ 8.0 CPU usage (raytracer).
* Linux desktop: TBD
