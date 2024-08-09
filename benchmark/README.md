# Node.js core benchmarks

This directory contains code and data used to evaluate the performance of various Node.js
implementations and JavaScript coding techniques executed by the built-in JavaScript engine.

For a comprehensive guide on writing and running benchmarks in this directory, refer to
[the guide on benchmarks][].

## File tree structure

### Directories

Benchmarks that test the performance of a single Node.js submodule are placed in a directory
named after that submodule, allowing for execution by submodule or individually. Benchmarks
that span multiple submodules may be placed in the `misc` directory or a directory named after
the feature they benchmark. For example, benchmarks for various new ECMAScript features and their
pre-ES2015 counterparts are located in the `es` directory. Fixtures that are reusable across the
benchmark suite should be placed in the `fixtures` directory.

### Other top-level files

The top-level files include common dependencies for the benchmarks and tools for launching benchmarks
and visualizing their output. The actual benchmark scripts should be placed in their respective directories.

## Common API

The `common.js` module provides consistency across benchmarks through various helpful functions
and properties.

### `createBenchmark(fn, configs[, options])`

Refer to [the guide on benchmarks][] for more details.

### `default_http_benchmarker`

The default benchmarker for running HTTP benchmarks. See [the guide on writing HTTP benchmarks][].

### `PORT`

The default port for running HTTP benchmarks. See [the guide on writing HTTP benchmarks][].

### `sendResult(data)`

Used in special benchmarks that cannot use `createBenchmark` and the object it returns.
This function reports timing data to the parent process, typically created by running
`compare.js`, `run.js`, or `scatter.js`.

[the guide on benchmarks]: ../doc/contributing/writing-and-running-benchmarks.md#basics-of-a-benchmark
[the guide on writing HTTP benchmarks]: ../doc/contributing/writing-and-running-benchmarks.md#creating-an-http-benchmark
