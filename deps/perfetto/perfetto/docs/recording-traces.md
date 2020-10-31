# Recording traces

Perfetto provides a few different ways for recording traces:

1. With the [Perfetto UI](#tracing-with-perfetto-ui). This is the most
   convenient way to get started.

2. With the [`perfetto` command line tool](#tracing-on-the-command-line) on
   Android. This is a good match for automated testing.

3. With the [Perfetto Client API](#tracing-with-the-api). This provides the
   most control when Perfetto is integrated in your app.

## Tracing with the API

> The [Perfetto SDK example](https://github.com/skyostil/perfetto-sdk-example)
> demonstrates trace recording through the API.

> Tracing is currently only supported with the in-process Perfetto service
> (perfetto::kInProcessBackend).

In order to record a trace, you should first initialize a
[TraceConfig](../protos/perfetto/config/trace_config.proto) message which
specifies what type of data to record. If your app includes track events
(i.e, `TRACE_EVENT`), you typically want to choose the categories which are
enabled for tracing. By default, all non-debug categories are enabled, but
you can enable a specific one like this:

```C++
perfetto::protos::gen::TrackEventConfig track_event_cfg;
track_event_cfg.add_disabled_categories("*");
track_event_cfg.add_enabled_categories("rendering");
```

Next, build the main trace config together with the track event part:

```C++
perfetto::TraceConfig cfg;
cfg.add_buffers()->set_size_kb(1024);  // Record up to 1 MiB.
auto* ds_cfg = cfg.add_data_sources()->mutable_config();
ds_cfg->set_name("track_event");
ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());
```

If your app includes a custom data source, you can also enable it here:

```C++
ds_cfg = cfg.add_data_sources()->mutable_config();
ds_cfg->set_name("my_data_source");
```

Read more about [advanced trace config features](trace-config.md). After
building the trace config, you can begin tracing:

```C++
std::unique_ptr<perfetto::TracingSession> tracing_session(
    perfetto::Tracing::NewTrace());
tracing_session->Setup(cfg);
tracing_session->StartBlocking();
```

> Tip: API methods with `Blocking` in their name will suspend the calling
> thread until the respective operation is complete. There are typically also
> asynchronous versions of each function to that don't have this limitation.

Now that tracing is active, instruct your app to perform the operation you
want to record. After that, we can stop tracing and collect the
protobuf-formatted trace data:

```C++
tracing_session->StopBlocking();
std::vector<char> trace_data(tracing_session->ReadTraceBlocking());

// Write the trace into a file.
std::ofstream output;
output.open("example.pftrace", std::ios::out | std::ios::binary);
output.write(&trace_data[0], trace_data.size());
output.close();
```

To save memory with longer traces, you can also tell Perfetto to write
directly into a file by passing a file descriptor into Setup(), remembering
to close the file after tracing is done:

```C++
int fd = open("example.pftrace", O_RDWR | O_CREAT | O_TRUNC, 0600);
tracing_session->Setup(cfg, fd);
tracing_session->StartBlocking();
// ...
tracing_session->StopBlocking();
close(fd);
```

The resulting trace file can be directly opened in the [Perfetto
UI](https://ui.perfetto.dev) or the [Trace Processor](trace-processor.md).

## Tracing with Perfetto UI

TODO(skyostil).

## Tracing on the command line

TODO(skyostil).
