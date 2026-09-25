# Heap Stats

Heap stats is a HTML-based tool for visualizing V8-internal object statistics.
For example, the tool can be used to visualize how much heap memory is used for
maintaining internal state versus actually allocated by the user.

The tool consumes log files produced by d8 (or Chromium) by passing
`--trace-gc-object-stats` or a trace captured using Chrome's tracing
infrastructure. Chrome trace files can either be processed as gzip or raw text
files.


Hosting requires a web server, e.g.:

    cd tools/heap-stats
    python -m SimpleHTTPServer 8000
