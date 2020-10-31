# Converting between trace formats

Perfetto traces can be converted into other trace formats using the
`traceconv` tool.

The formats supported today are:
 * proto text format: the standard text based representation of protos
 * Chrome JSON format: the format used by chrome://tracing
 * systrace format: the ftrace text format used by Android systrace
 * profile format (heap profiler only): pprof-like format.
   This is only valid for traces with
   [heap profiler](src/profiling/memory/README.md) dumps.

traceconv is also used in the UI to convert Perfetto traces to the Chrome
JSON format and directly open these traces in the legacy systrace UI
(Catapult's chrome://tracing).

Usage
---------
```
curl https://get.perfetto.dev/traceconv -o traceconv
chmod +x traceconv
./traceconv [text|json|systrace|profile] [input proto file] [output file]
```

Examples
---------

### Converting a perfetto trace to systrace text format
`./traceconv systrace [input proto file] [output systrace file]`

### Opening a Perfetto trace in the legacy systrace UI
Navigate to ui.perfetto.dev and choose the "Open with legacy UI" option. This
runs traceconv (the progress of which can be seen in the UI) and passes the
converted trace seamlessly to chrome://tracing

### Converting a perfetto trace to Chrome JSON format (for chrome://tracing)
`./traceconv json [input proto file] [output json file]`
