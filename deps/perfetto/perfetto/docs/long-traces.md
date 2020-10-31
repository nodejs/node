# Long traces with Perfetto

By default Perfetto keeps the full trace buffer in memory and writes it into the
destination file (passed with the `-o` cmdline argument) only at the end of the
trace, to reduce the intrusiveness of the tracing system.
That, however, limits the max size of the trace to the physical memory size of
the device.

In some cases (e.g., benchmarks, hard to repro cases) it is desirable to capture
traces that are way larger than that.


To achieve that, Perfetto allows to periodically flush the trace buffers into
the target file (or stdout) by using some flags in the
[`TraceConfig`](/protos/perfetto/config/trace_config.proto), specifically:

`bool write_into_file`  
When true drains periodically the trace buffers into the output
file. When this option is enabled, the userspace buffers need to be just
big enough to hold tracing data between two periods.
The buffer sizing depends on the activity of the device. A reasonable estimation
is ~5-20 MB per second.

`uint32 file_write_period_ms`  
Overrides the default drain period. Shorter periods require a smaller userspace
buffer but increase the performance intrusiveness of tracing. A minimum interval
of 100ms is enforced.

`uint64 max_file_size_bytes`  
If set, stops the tracing session after N bytes have been written. Used to
cap the size of the trace.

For a complete example of a working trace config in long-tracing mode see
[`/test/configs/long_trace.cfg`](/test/configs/long_trace.cfg)

## Instructions
These instructions assume you have a working standalone checkout (see
[instructions here](/docs/build-instructions.md)).

These instructions have been tested as non-root. Many of the steps below can be
simplified when running as root and are required due to SELinux when running as
`shell` rather than `root`.

``` bash
$ cd perfetto

# Prepare for the build (as per instructions linked above).
$ tools/install-build-deps
$ tools/gn gen out/mac_release --args="is_debug=false"

# Compiles the textual protobuf into binary format
# for /test/configs/long_trace.cfg.
$ tools/ninja -C out/mac_release/ long_trace.cfg.protobuf

# Alternatively, the more verbose variant:
$ alias protoc=$(pwd)/out/mac_release/protoc
$ protoc --encode=perfetto.protos.TraceConfig \
        -I$(pwd) \
        $(pwd)/protos/perfetto/config/perfetto_config.proto \
        < /test/configs/long_trace.cfg \
        > /tmp/long_trace.cfg.protobuf

# Push the config onto the device.
$ adb push out/mac_release/long_trace.cfg.protobuf /data/local/tmp/long_trace.cfg.protobuf

# Run perfetto.
# Note: Unless running as root, the output folder must be under
# /data/misc/perfetto-traces/, or SELinux will cause a failure.
$ adb shell 'cat /data/local/tmp/long_trace.cfg.protobuf | perfetto -c - -o /data/misc/perfetto-traces/trace --background'

# At this point the trace will start in background. It is possible to detach the
# usb cable. In order to verify if the trace is still ongoing, just run:
$ adb shell ps -A | grep perfetto

# While it's running, you should see an entry like this:
shell        23705     1 2166232  12344 do_sys_poll 7796ef2700 S perfetto

# At the end of the trace, the process will be gone. At this point it is
# possible to pull the trace.
$ adb shell gzip -c /data/misc/perfetto-traces/trace -3 | gzip -dc - > ~/trace

# Verify that the trace has not been corrupted by the adb transfer
$ adb shell sha1sum /data/misc/perfetto-traces/trace
b9f7a7e3d62638b5d9e880db30f68787c458bb3c  /data/misc/perfetto-traces/trace

$ shasum  ~/trace
b9f7a7e3d62638b5d9e880db30f68787c458bb3c  /Users/primiano/trace
```

At this point it is possible to load / process the trace as explained below.

### Get high-level trace stats
Use the `trace_to_text` binary in `summary` mode. This allows to detect whether
the buffers were sized appropriatel or any overrun happened, either in the
kernel ftrace buffer or in the userspace buffer. Look for
`Events overwritten`, `total_overrun` (kernel ftrace buffer)
and `chunks_overwritten` (userspace buffer).

`trace_to_text` can be also used to convert the trace from the protobuf format
to systrace textual version (see `trace_to_text --help`).


``` bash
$ ninja -C out/mac_release trace_to_text

$ out/mac_release/trace_to_text summary < ~/trace
Ftrace duration: 29737ms
Boottime duration: 21000ms
-------------------- ftrace --------------------
▁▄▅▄▄▂▁▄▄▄▅▅▃▄▅▅▃▂▂▁▁▃▄▄▁▁▄▁▁ ▁▁▂▄▄▂▃▂▂▂▁▂▄▁▂▁▁▁

--------------------Ftrace Stats-------------------
Events overwritten: 0
total_overrun: 0 (= 0 - 0)

----------------Process Tree Stats----------------
Unique thread ids in process tree: 1372
Unique thread ids in ftrace events: 1580
Thread ids with process info: 1325/1580 -> 83 %

--------------------Trace Stats-------------------
Buffer 0
  bytes_written: 37074960
  chunks_written: 9087
  chunks_overwritten: 0
```

### Load the trace in the UI

*** note
**The UI and trace processor are WIP, targeting end of Q3-18**.
<!-- TODO(primiano): update this doc. -->
***

Open https://ui.perfetto.dev and load the trace there.

### Load the trace in the trace processor
``` bash
$ ninja -C out/mac_release trace_processor
$ out/mac_release/trace_processor_shell ~/trace

trace_processor_shell.cc Trace loaded: 1048.58 MB (197.1 MB/s)

> select tdur/1e9 as runtime_sec, name from (select sum(dur) as tdur, utid from sched group by utid) inner join thread using(utid) order by tdur desc limit 20
         runtime_sec                 name
-------------------- --------------------
         2560.695682 PowerManagerSer
         2532.212882 migration/2
         2529.064936 migration/3
         2527.338100 migration/1
         2526.877703 migration/4
         2524.508852 migration/5
         2523.372052 migration/6
         2522.564051 migration/7
          100.533405 traced_probes
           80.585233 Binder:1229_E
           46.294051 Chrome_IOThread
           35.251236 .gms.persistent
           27.716663 SensorService
           26.818083 CrGpuMain
           26.199466 m.chrome.canary
           25.766977 CrRendererMain
           25.623455 CrRendererMain
           25.429535 android.bg
           25.060462 traced
           23.140243 lowpool[2633]
```
