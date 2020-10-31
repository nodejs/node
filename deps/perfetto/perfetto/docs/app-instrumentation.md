# App instrumentation

The Perfetto Client API is a C++ library that allows applications to emit
trace events to add more context to a Perfetto trace to help with
development, debugging and performance analysis.

> The code from this example is also available as a [GitHub repository](
> https://github.com/skyostil/perfetto-sdk-example).

To start using the Client API, first check out the latest SDK release:

```sh
$ git clone https://android.googlesource.com/platform/external/perfetto -b latest
```

The SDK consists of two files, `sdk/perfetto.h` and
`sdk/perfetto.cc`. These are an amalgamation of the Client API designed to
easy to integrate to existing build systems. For example, to add the SDK to a
CMake project, edit your `CMakeLists.txt` accordingly:

```cmake
cmake_minimum_required(VERSION 3.13)
project(PerfettoExample)
find_package(Threads)

# Define a static library for Perfetto.
include_directories(perfetto/sdk)
add_library(perfetto STATIC perfetto/sdk/perfetto.cc)

# Link the library to your main executable.
add_executable(example example.cc)
target_link_libraries(example perfetto ${CMAKE_THREAD_LIBS_INIT})
```

Next, initialize Perfetto in your program:

```C++

#include <perfetto.h>

int main(int argv, char** argc) {
  perfetto::TracingInitArgs args;

  // The backends determine where trace events are recorded. You may select one
  // or more of:

  // 1) The in-process backend only records within the app itself.
  args.backends |= perfetto::kInProcessBackend;

  // 2) The system backend writes events into a system Perfetto daemon,
  //    allowing merging app and system events (e.g., ftrace) on the same
  //    timeline. Requires the Perfetto `traced` daemon to be running (e.g.,
  //    on Android Pie and newer).
  args.backends |= perfetto::kSystemBackend;

  perfetto::Tracing::Initialize(args);
}
```

You are now ready to instrument your app with trace events. The Client API
has two options for this:

- [Track events](#track-events), which represent time-bounded operations
   (e.g., function calls) on a timeline. Track events are a good choice for
   most apps.

- [Custom data sources](#custom-data-sources), which can be used to
   efficiently record arbitrary app-defined data using a protobuf encoding.
   Custom data sources are a typically better match for advanced Perfetto
   users.

# Track events

![Track events shown in the Perfetto UI](
  track-events.png "Track events in the Perfetto UI")

*Track events* are application specific, time bounded events recorded into a
*trace* while the application is running. Track events are always associated
with a *track*, which is a timeline of monotonically increasing time. A track
corresponds to an independent sequence of execution, such as a single thread
in a process.

There are a few main types of track events:

1. **Slices**, which represent nested, time bounded operations. For example,
  a slice could cover the time period from when a function begins executing
  to when it returns, the time spent loading a file from the network or the
  time spent blocked on a disk read.

2. **Counters**, which are snapshots of time-varying numeric values. For
  example, a track event can record instantaneous the memory usage of a
  process during its execution.

3. **Flows**, which are used to connect related slices that span different
  tracks together. For example, if an image file is first loaded from
  the network and then decoded on a thread pool, a flow event can be used to
  highlight its path through the system. (Not fully implemented yet).

The [Perfetto UI](https://ui.perfetto.dev) has built in support for track
events, which provides a useful way to quickly visualize the internal
processing of an app. For example, the [Chrome
browser](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool)
is deeply instrumented with track events to assist in debugging, development
and performance analysis.

A typical use case for track events is annotating a function with a scoped
track event, so that function's execution shows up in a trace. To start using
track events, first define the set of categories that your events will fall
into. Each category can be separately enabled or disabled for tracing (see
[Category configuration](#category-configuration).

Add the list of categories into a header file (e.g., `example_tracing.h`)
like this:

```C++
#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("rendering")
        .SetDescription("Events from the graphics subsystem"),
    perfetto::Category("network")
        .SetDescription("Network upload and download statistics"));
```

Then, declare static storage for the categories in a cc file (e.g.,
`example_tracing.cc`):

```C++
#include "example_tracing.h"

PERFETTO_TRACK_EVENT_STATIC_STORAGE();
```

Finally, initialize track events after the client library is brought up:

```C++
int main(int argv, char** argc) {
  ...
  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();  // Add this.
}
```

Now you can add track events to existing functions like this:

```C++
#include "example_tracing.h"

void DrawPlayer() {
  TRACE_EVENT("rendering", "DrawPlayer");
  ...
}
```

This type of trace event is scoped, which means it will cover the time from
when the function began executing until the point of return. You can also
supply (up to two) debug annotations together with the event.

```C++
int player_number = 1;
TRACE_EVENT("rendering", "DrawPlayer", "player_number", player_number);
```

For more complex arguments, you can define [your own protobuf
messages](../protos/perfetto/trace/track_event/track_event.proto) and emit
them as a parameter for the event.

> Currently custom protobuf messages need to be added directly to the
> Perfetto repository under `protos/perfetto/trace`, and Perfetto itself must
> also be rebuilt. We are working [to lift this
> limitation](https://github.com/google/perfetto/issues/11).

As an example of a custom track event argument type, save the following as
`protos/perfetto/trace/track_event/player_info.proto`:

```protobuf
message PlayerInfo {
  optional string name = 1;
  optional uint64 score = 2;
}
```

This new file should also be added to
`protos/perfetto/trace/track_event/BUILD.gn`:

```json
sources = [
  ...
  "player_info.proto"
]
```

Also, a matching argument should be added to the track event message
definition in
`protos/perfetto/trace/track_event/track_event.proto`:

```protobuf
import "protos/perfetto/trace/track_event/player_info.proto";

...

message TrackEvent {
  ...
  // New argument types go here :)
  optional PlayerInfo player_info = 1000;
}
```

The corresponding trace point could look like this:

```C++
Player my_player;
TRACE_EVENT("category", "MyEvent", [&](perfetto::EventContext ctx) {
  auto player = ctx.event()->set_player_info();
  player->set_name(my_player.name());
  player->set_player_score(my_player.score());
});
```

The lambda function passed to the macro is only called if tracing is enabled for
the given category. It is always called synchronously and possibly multiple
times if multiple concurrent tracing sessions are active.

Now that you have instrumented your app with track events, you are ready to
start [recording traces](recording-traces.md).

## Category configuration

All track events are assigned to one more trace categories. For example:

```C++
TRACE_EVENT("rendering", ...);  // Event in the "rendering" category.
```

By default, all non-debug and non-slow track event categories are enabled for
tracing. *Debug* and *slow* categories are categories with special tags:

  - `"debug"` categories can give more verbose debugging output for a particular
    subsystem.
  - `"slow"` categories record enough data that they can affect the interactive
    performance of your app.

Category tags can be can be defined like this:

```C++
perfetto::Category("rendering.debug")
    .SetDescription("Debug events from the graphics subsystem")
    .SetTags("debug", "my_custom_tag")
```

A single trace event can also belong to multiple categories:

```C++
// Event in the "rendering" and "benchmark" categories.
TRACE_EVENT("rendering,benchmark", ...);
```

A corresponding category group entry must be added to the category registry:

```C++
perfetto::Category::Group("rendering,benchmark")
```

It's also possible to efficiently query whether a given category is enabled
for tracing:

```C++
if (TRACE_EVENT_CATEGORY_ENABLED("rendering")) {
  // ...
}
```

The `TrackEventConfig` field in Perfetto's `TraceConfig` can be used to
select which categories are enabled for tracing:

```protobuf
message TrackEventConfig {
  // Each list item is a glob. Each category is matched against the lists
  // as explained below.
  repeated string disabled_categories = 1;  // Default: []
  repeated string enabled_categories = 2;   // Default: []
  repeated string disabled_tags = 3;        // Default: [“slow”, “debug”]
  repeated string enabled_tags = 4;         // Default: []
}
```

To determine if a category is enabled, it is checked against the filters in the
following order:

1. Exact matches in enabled categories.
2. Exact matches in enabled tags.
3. Exact matches in disabled categories.
4. Exact matches in disabled tags.
5. Pattern matches in enabled categories.
6. Pattern matches in enabled tags.
7. Pattern matches in disabled categories.
8. Pattern matches in disabled tags.

If none of the steps produced a match, the category is enabled by default. In
other words, every category is implicitly enabled unless specifically disabled.
For example:

| Setting                         | Needed configuration                         |
| ------------------------------- | -------------------------------------------- |
| Enable just specific categories | `enabled_categories = [“foo”, “bar”, “baz”]` |
|                                 | `disabled_categories = [“*”]`                |
| Enable all non-slow categories  | (Happens by default.)                        |
| Enable specific tags            | `disabled_tags = [“*”]`                      |
|                                 | `enabled_tags = [“foo”, “bar”]`              |

## Dynamic and test-only categories

Ideally all trace categories should be defined at compile time as shown
above, as this ensures trace points will have minimal runtime and binary size
overhead. However, in some cases trace categories can only be determined at
runtime (e.g., by JavaScript). These can be used by trace points as follows:

```C++
perfetto::DynamicCategory dynamic_category{"nodejs.something"};
TRACE_EVENT(dynamic_category, "SomeEvent", ...);
```

> Tip: It's also possible to use dynamic event names by passing `nullptr` as
> the name and filling in the `TrackEvent::name` field manually.

Some trace categories are only useful for testing, and they should not make
it into a production binary. These types of categories can be defined with a
list of prefix strings:

```C++
PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES(
   "test",
   "cat"
);
```

# Custom data sources

For most uses, track events are the most straightforward way of instrumenting
your app for tracing. However, in some rare circumstances they are not
flexible enough, e.g., when the data doesn't fit the notion of a track or is
high volume enough that it need strongly typed schema to minimize the size of
each event. In this case, you can implement a *custom data source* for
Perfetto.

Note that when working with custom data sources, you will also need
corresponding changes in [trace processor](trace-processor.md) to enable
importing your data format.

A custom data source is a subclass of `perfetto::DataSource`. Perfetto with
automatically create one instance of the class for each tracing session it is
active in (usually just one).

```C++
class CustomDataSource : public perfetto::DataSource<CustomDataSource> {
 public:
  void OnSetup(const SetupArgs&) override {
    // Use this callback to apply any custom configuration to your data source
    // based on the TraceConfig in SetupArgs.
  }

  void OnStart(const StartArgs&) override {
    // This notification can be used to initialize the GPU driver, enable
    // counters, etc. StartArgs will contains the DataSourceDescriptor,
    // which can be extended.
  }

  void OnStop(const StopArgs&) override {
    // Undo any initialization done in OnStart.
  }

  // Data sources can also have per-instance state.
  int my_custom_state = 0;
};

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(CustomDataSource);
```

The data source's static data should be defined in one source file like this:

```C++
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(CustomDataSource);
```

Custom data sources need to be registered with Perfetto:

```C++
int main(int argv, char** argc) {
  ...
  perfetto::Tracing::Initialize(args);
  // Add the following:
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("com.example.custom_data_source");
  CustomDataSource::Register(dsd);
}
```

As with all data sources, the custom data source needs to be specified in the
trace config to enable tracing:

```C++
perfetto::TraceConfig cfg;
auto* ds_cfg = cfg.add_data_sources()->mutable_config();
ds_cfg->set_name("com.example.custom_data_source");
```

Finally, call the `Trace()` method to record an event with your custom data
source. The lambda function passed to that method will only be called if tracing
is enabled. It is always called synchronously and possibly multiple times if
multiple concurrent tracing sessions are active.

```C++
CustomDataSource::Trace([](CustomDataSource::TraceContext ctx) {
  auto packet = ctx.NewTracePacket();
  packet->set_timestamp(perfetto::TrackEvent::GetTraceTimeNs());
  packet->set_for_testing()->set_str("Hello world!");
});
```

If necessary the `Trace()` method can access the custom data source state
(`my_custom_state` in the example above). Doing so, will take a mutex to
ensure data source isn't destroyed (e.g., because of stopping tracing) while
the `Trace()` method is called on another thread. For example:

```C++
CustomDataSource::Trace([](CustomDataSource::TraceContext ctx) {
  auto safe_handle = trace_args.GetDataSourceLocked();  // Holds a RAII lock.
  DoSomethingWith(safe_handle->my_custom_state);
});
```

# Performance

Perfetto's trace points are designed to have minimal overhead when tracing is
disabled while providing high throughput for data intensive tracing use
cases. While exact timings will depend on your system, there is a
[microbenchmark](../src/tracing/api_benchmark.cc) which gives some ballpark
figures:

| Scenario | Runtime on Pixel 3 XL | Runtime on ThinkStation P920 |
| -------- | --------------------- | ---------------------------- |
| `TRACE_EVENT(...)` (disabled)              | 2 ns   | 1 ns   |
| `TRACE_EVENT("cat", "name")`               | 285 ns | 630 ns |
| `TRACE_EVENT("cat", "name", <lambda>)`     | 304 ns | 663 ns |
| `TRACE_EVENT("cat", "name", "key", value)` | 354 ns | 664 ns |
| `DataSource::Trace(<lambda>)` (disabled)   | 2 ns   | 1 ns   |
| `DataSource::Trace(<lambda>)`              | 133 ns | 58 ns  |

# Advanced topics

## Tracks

Every track event is associated with a track, which specifies the timeline
the event belongs to. In most cases, a track corresponds to a visual
horizontal track in the Perfetto UI like this:

![Track timelines shown in the Perfetto UI](
  track-timeline.png "Track timelines in the Perfetto UI")

Events that describe parallel sequences (e.g., separate
threads) should use separate tracks, while sequential events (e.g., nested
function calls) generally belong on the same track.

Perfetto supports three kinds of tracks:

1. `Track` – a basic timeline.

2. `ProcessTrack` – a timeline that represents a single process in the system.

3. `ThreadTrack` – a timeline that represents a single thread in the system.

Tracks can have a parent track, which is used to group related tracks
together. For example, the parent of a `ThreadTrack` is the `ProcessTrack` of
the process the thread belongs to. By default, tracks are grouped under the
current process's `ProcessTrack`.

A track is identified by a uuid, which must be unique across the entire
recorded trace. To minimize the chances of accidental collisions, the uuids
of child tracks are combined with those of their parents, with each
`ProcessTrack` having a random, per-process uuid.

By default, track events (e.g., `TRACE_EVENT`) use the `ThreadTrack` for the
calling thread. This can be overridden, for example, to mark events that
begin and end on a different thread:

```C++
void OnNewRequest(size_t request_id) {
  // Open a slice when the request came in.
  TRACE_EVENT_BEGIN("category", "HandleRequest", perfetto::Track(request_id));

  // Start a thread to handle the request.
  std::thread worker_thread([=] {
    // ... produce response ...

    // Close the slice for the request now that we finished handling it.
    TRACE_EVENT_END("category", perfetto::Track(request_id));
  });
```
Tracks can also optionally be annotated with metadata:

```C++
perfetto::TrackEvent::SetTrackDescriptor(
    track, [](perfetto::protos::pbzero::TrackDescriptor* desc) {
  desc->set_name("MyTrack");
});
```

The metadata remains valid between tracing sessions. To free up data for a
track, call EraseTrackDescriptor:

```C++
perfetto::TrackEvent::EraseTrackDescriptor(track);
```

## Interning

Interning can be used to avoid repeating the same constant data (e.g., event
names) throughout the trace. Perfetto automatically performs interning for
most strings passed to `TRACE_EVENT`, but it's also possible to also define
your own types of interned data.

First, define an interning index for your type. It should map to a specific
field of
[interned_data.proto](../protos/perfetto/trace/interned_data/interned_data.proto)
and specify how the interned data is written into that message when seen for
the first time.

```C++
struct MyInternedData
    : public perfetto::TrackEventInternedDataIndex<
        MyInternedData,
        perfetto::protos::pbzero::InternedData::kMyInternedDataFieldNumber,
        const char*> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                   size_t iid,
                   const char* value) {
    auto my_data = interned_data->add_my_interned_data();
    my_data->set_iid(iid);
    my_data->set_value(value);
  }
};
```

Next, use your interned data in a trace point as shown below. The interned
string will only be emitted the first time the trace point is hit (unless the
trace buffer has wrapped around).

```C++
TRACE_EVENT(
   "category", "Event", [&](perfetto::EventContext ctx) {
     auto my_message = ctx.event()->set_my_message();
     size_t iid = MyInternedData::Get(&ctx, "Repeated data to be interned");
     my_message->set_iid(iid);
   });
```

Note that interned data is strongly typed, i.e., each class of interned data
uses a separate namespace for identifiers.

## Counters

TODO(skyostil).

## Flow events

TODO(skyostil).
