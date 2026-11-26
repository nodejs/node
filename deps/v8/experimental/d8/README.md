# Experimental d8 extensions

Prototypes that plug into d8. None of this is built by default.

## mock-dom

A small native DOM for d8 (`d8 --experimental-mock-dom`), for running
DOM-heavy web workloads such as Speedometer without a browser. Build it with
the GN arg `v8_enable_experimental_mock_dom = true`. See
[mock-dom/mock-dom.h](mock-dom/mock-dom.h).

## speedometer-harness

Runs the unmodified Speedometer 2.1, 3.1 and main on the mock DOM and checks
the DOM each workload builds against summaries recorded in Chrome:

```
out/x64.release/d8 --experimental-mock-dom \
    experimental/d8/speedometer-harness/run-speedometer.js \
    -- [--version=2.1|3.1|main] [--iterations=N] [SuiteName...]
```

The Speedometer checkouts come from DEPS (`checkout_benchmarks`), under
`test/benchmarks/`.
