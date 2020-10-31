# Perfetto trace config

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): write trace config doc. -->
***

![Trace Config](https://storage.googleapis.com/perfetto/markdown_img/trace-config.png)

The [`TraceConfig`](/protos/perfetto/config/trace_config.proto) is an extensible
protobuf message, sent by the consumer to the service, that defines:
- The number and size of the trace buffer.
- The duration of the trace.
- [optionally] a file descriptor for the output trace and a periodic write
  interval. If omitted the trace is kept only in memory.  
- The producers involved in the trace session.
- The data sources involved in the trace session.
- The configuration of each data source.
- The crossbar mapping between each data source and the trace buffers.

Each data source can create its own specialized schema for the config, like
[this](/protos/perfetto/config/ftrace/ftrace_config.proto)

See [`trace_config.proto`](/protos/perfetto/config/trace_config.proto) for more
details.

For convenience, a vulcanized trace config where all the nested protobuf
sub-message definitions are squashed together is available in
[`perfetto_config.proto`](/protos/perfetto/config/perfetto_config.proto).


Specifying a custom trace config
--------------------------------
```bash
cat > /tmp/config.txpb <<EOF
# This is a text-encoded protobuf for /protos/perfetto/config/trace_config.proto
duration_ms: 10000

# For long traces set the following variables. It will periodically drain the
# trace buffers into the output file, allowing to save a trace larger than the
# buffer size.
write_into_file: true
file_write_period_ms: 5000

buffers {
  size_kb: 10240
}

data_sources {
  config {
    name: "linux.ftrace"
    target_buffer: 0
    ftrace_config {
      buffer_size_kb: 40 # Kernel ftrace buffer size.
      ftrace_events: "sched_switch"
      ftrace_events: "print"
    }
  }
}

data_sources {
  config {
    name: "linux.process_stats"
    target_buffer: 0
  }
}
EOF

protoc=$(pwd)/out/android/gcc_like_host/protoc

$protoc --encode=perfetto.protos.TraceConfig \
        -I$(pwd)/external/perfetto \
        $(pwd)/external/perfetto/protos/perfetto/config/perfetto_config.proto \
        < /tmp/config.txpb \
        > /tmp/config.pb

cat /tmp/config.pb | adb shell perfetto -c - -o /data/misc/perfetto-traces/trace.pb
adb shell cat /data/misc/perfetto-traces/trace.pb > /tmp/trace.pb
out/android/trace_to_text json < /tmp/trace.pb > /tmp/trace.json

# The file can now be viewed in chrome://tracing
```
