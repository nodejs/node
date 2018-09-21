# Node.js Core Benchmarks

This folder contains code and data used to measure performance
of different Node.js implementations and different ways of
writing JavaScript run by the built-in JavaScript engine.

For a detailed guide on how to write and run benchmarks in this
directory, see [the guide on benchmarks](../doc/guides/writing-and-running-benchmarks.md).

## Table of Contents

* [Benchmark directories](#benchmark-directories)
* [Common API](#common-api)

## Benchmark Directories

<table>
  <thead>
    <tr>
      <th>Directory</th>
      <th>Purpose</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>arrays</td>
      <td>
        Benchmarks for various operations on array-like objects,
        including <code>Array</code>, <code>Buffer</code>, and typed arrays.
      </td>
    </tr>
    <tr>
      <td>assert</td>
      <td>
        Benchmarks for the <code>assert</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>buffers</td>
      <td>
        Benchmarks for the <code>buffer</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>child_process</td>
      <td>
        Benchmarks for the <code>child_process</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>crypto</td>
      <td>
        Benchmarks for the <code>crypto</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>dgram</td>
      <td>
        Benchmarks for the <code>dgram</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>domain</td>
      <td>
        Benchmarks for the <code>domain</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>es</td>
      <td>
        Benchmarks for various new ECMAScript features and their
        pre-ES2015 counterparts.
      </td>
    </tr>
    <tr>
      <td>events</td>
      <td>
        Benchmarks for the <code>events</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>fixtures</td>
      <td>
        Benchmarks fixtures used in various benchmarks throughout
        the benchmark suite.
      </td>
    </tr>
    <tr>
      <td>fs</td>
      <td>
        Benchmarks for the <code>fs</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>http</td>
      <td>
        Benchmarks for the <code>http</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>http2</td>
      <td>
        Benchmarks for the <code>http2</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>misc</td>
      <td>
        Miscellaneous benchmarks and benchmarks for shared
        internal modules.
      </td>
    </tr>
    <tr>
      <td>module</td>
      <td>
        Benchmarks for the <code>module</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>net</td>
      <td>
        Benchmarks for the <code>net</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>path</td>
      <td>
        Benchmarks for the <code>path</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>process</td>
      <td>
        Benchmarks for the <code>process</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>querystring</td>
      <td>
        Benchmarks for the <code>querystring</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>streams</td>
      <td>
        Benchmarks for the <code>streams</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>string_decoder</td>
      <td>
        Benchmarks for the <code>string_decoder</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>timers</td>
      <td>
        Benchmarks for the <code>timers</code> subsystem, including
        <code>setTimeout</code>, <code>setInterval</code>, .etc.
      </td>
    </tr>
    <tr>
      <td>tls</td>
      <td>
        Benchmarks for the <code>tls</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>url</td>
      <td>
        Benchmarks for the <code>url</code> subsystem, including the legacy
        <code>url</code> implementation and the WHATWG URL implementation.
      </td>
    </tr>
    <tr>
      <td>util</td>
      <td>
        Benchmarks for the <code>util</code> subsystem.
      </td>
    </tr>
    <tr>
      <td>vm</td>
      <td>
        Benchmarks for the <code>vm</code> subsystem.
      </td>
    </tr>
  </tbody>
</table>

### Other Top-level files

The top-level files include common dependencies of the benchmarks
and the tools for launching benchmarks and visualizing their output.
The actual benchmark scripts should be placed in their corresponding
directories.

* `_benchmark_progress.js`: implements the progress bar displayed
  when running `compare.js`
* `_cli.js`: parses the command line arguments passed to `compare.js`,
  `run.js` and `scatter.js`
* `_cli.R`: parses the command line arguments passed to `compare.R`
* `_http-benchmarkers.js`: selects and runs external tools for benchmarking
  the `http` subsystem.
* `common.js`: see [Common API](#common-api).
* `compare.js`: command line tool for comparing performance between different
  Node.js binaries.
* `compare.R`: R script for statistically analyzing the output of
  `compare.js`
* `run.js`: command line tool for running individual benchmark suite(s).
* `scatter.js`: command line tool for comparing the performance
  between different parameters in benchmark configurations,
  for example to analyze the time complexity.
* `scatter.R`: R script for visualizing the output of `scatter.js` with
  scatter plots.

## Common API

The common.js module is used by benchmarks for consistency across repeated
tasks. It has a number of helpful functions and properties to help with
writing benchmarks.

### createBenchmark(fn, configs[, options])

See [the guide on writing benchmarks](../doc/guides/writing-and-running-benchmarks.md#basics-of-a-benchmark).

### default\_http\_benchmarker

The default benchmarker used to run HTTP benchmarks.
See [the guide on writing HTTP benchmarks](../doc/guides/writing-and-running-benchmarks.md#creating-an-http-benchmark).


### PORT

The default port used to run HTTP benchmarks.
See [the guide on writing HTTP benchmarks](../doc/guides/writing-and-running-benchmarks.md#creating-an-http-benchmark).

### sendResult(data)

Used in special benchmarks that can't use `createBenchmark` and the object
it returns to accomplish what they need. This function reports timing
data to the parent process (usually created by running `compare.js`, `run.js` or
`scatter.js`).

