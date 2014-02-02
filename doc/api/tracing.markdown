# Tracing

    Stability: 1 - Experimental

The tracing module is designed for instrumenting your Node application. It is
not meant for general purpose use.

***Be very careful with callbacks used in conjunction with this module***

Many of these callbacks interact directly with asynchronous subsystems in a
synchronous fashion. That is to say, you may be in a callback where a call to
`console.log()` could result in an infinite recursive loop.  Also of note, many
of these callbacks are in hot execution code paths. That is to say your
callbacks are executed quite often in the normal operation of Node, so be wary
of doing CPU bound or synchronous workloads in these functions. Consider a ring
buffer and a timer to defer processing.

`require('tracing')` to use this module.

## v8

The `v8` property is an [EventEmitter][], it exposes events and interfaces
specific to the version of `v8` built with node. These interfaces are subject
to change by upstream and are therefore not covered under the stability index.

### Event: 'gc'

`function (before, after) { }`

Emitted each time a GC run is completed.

`before` and `after` are objects with the following properties:

```
{
  type: 'mark-sweep-compact',
  flags: 0,
  timestamp: 905535650119053,
  total_heap_size: 6295040,
  total_heap_size_executable: 4194304,
  total_physical_size: 6295040,
  used_heap_size: 2855416,
  heap_size_limit: 1535115264
}
```

### getHeapStatistics()

Returns an object with the following properties

```
{
  total_heap_size: 7326976,
  total_heap_size_executable: 4194304,
  total_physical_size: 7326976,
  used_heap_size: 3476208,
  heap_size_limit: 1535115264
}
```

[EventEmitter]: events.html#events_class_events_eventemitter
