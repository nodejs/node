# Tracing

    Stability: 1 - Experimental

The tracing module is designed for instrumenting your Node application. It is
not meant for general purpose use.

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

### setFlagsFromString()

Set additional V8 command line flags.  Use with care; changing settings
after the VM has started may result in unpredictable behavior, including
crashes and data loss.  Or it may simply do nothing.

Usage:

```
// Print GC events to stdout for one minute.
var v8 = require('tracing').v8;
v8.setFlagsFromString('--trace_gc');
setTimeout(function() { v8.setFlagsFromString('--notrace_gc'); }, 60e3);
```

 
