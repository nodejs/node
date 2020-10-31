# Java Heap Profiling

**Java Heap Profiling requires Android 11.**

Java Heap Profiling allows you to capture a snapshot of the memory use of
objects managed by ART (Android RunTime). This allows to debug situations
where a lot of memory is used on the managed heap.

## Quickstart

To grab a profile from your device, run the following command, substituting
`YOUR_APP_NAME` with the name of the app you want to profile.

```
 echo 'buffers {
  size_kb: 102400
  fill_policy: RING_BUFFER
}

data_sources {
  config {
    name: "android.java_hprof"
    java_hprof_config {
      process_cmdline: "YOUR_APP_NAME"
    }
  }
}

duration_ms: 10000
write_into_file: true
' | adb shell perfetto -c - --out /data/misc/perfetto-traces/profile --txt
```

Then, pull the data onto your machine.

```
adb pull /data/misc/perfetto-traces/profile some/path
```

## Viewing the data

Upload the trace to the [Perfetto UI](https://ui.perfetto.dev) and click on
diamond marker that shows.

This will present a flamegraph of the memory attributed to the shortest path
to a garbage-collection root. In general an object is reachable by many paths,
we only show the shortest as that reduces the complexity of the data displayed
and is generally the highest-signal.

We aggregate the paths per class name, so if there are two `Foo` objects that
each retain a `String`, we will show one element for `String` as a child of
one `Foo`.
