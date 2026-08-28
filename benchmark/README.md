# Node.js Core Benchmarks

This folder contains code and data used to measure performance
of different Node.js implementations and different ways of
writing JavaScript run by the built-in JavaScript engine.

For a detailed guide on how to write and run benchmarks in this
directory, see [the guide on benchmarks](../doc/contributing/writing-and-running-benchmarks.md).

## Table of Contents

* [File tree structure](#file-tree-structure)
* [`node:bench` evaluation tools](#nodebench-evaluation-tools)
* [Common API](#common-api)

## File tree structure

### Directories

Benchmarks testing the performance of a single node submodule are placed into a
directory with the corresponding name, so that they can be executed by submodule
or individually.
Benchmarks that span multiple submodules may either be placed into the `misc`
directory or into a directory named after the feature they benchmark.
E.g. benchmarks for various new ECMAScript features and their pre-ES2015
counterparts are placed in a directory named `es`.
Fixtures that are not specific to a certain benchmark but can be reused
throughout the benchmark suite should be placed in the `fixtures` directory.

### Other Top-level files

The top-level files include common dependencies of the benchmarks
and the tools for launching benchmarks and visualizing their output.
The actual benchmark scripts should be placed in their corresponding
directories.

* `_benchmark_progress.js`: implements the progress bar displayed
  when running `compare.js` and `scatter.js`
* `_cli.js`: parses the command line arguments passed to `compare.js`,
  `run.js` and `scatter.js`
* `_cli.R`: parses the command line arguments passed to `compare.R`
* `_http-benchmarkers.js`: selects and runs external tools for benchmarking
  the `http` subsystem.
* `bar.R`: R script for visualizing the output of benchmarks with bar plots.
* `common.js`: see [Common API](#common-api).
* `compare.js`: command line tool for comparing performance between different
  Node.js binaries.
* `compare-node-bench.js`: parallel comparison tool for explicit `node:bench`
  files. It does not change or invoke `compare.js`.
* `compare.R`: R script for statistically analyzing the output of
  `compare.js`
* `run.js`: command line tool for running individual benchmark suite(s).
* `scatter.js`: command line tool for comparing the performance
  between different parameters in benchmark configurations,
  for example to analyze the time complexity. Pass `--analyze` to
  summarize the results without R.
* `scatter-node-bench.js`: parallel scatter-data tool for an explicit
  `node:bench` file. It does not change or invoke `scatter.js`.
* `scatter.R`: R script for visualizing the output of `scatter.js` with
  scatter plots.

## `node:bench` evaluation tools

The `compare-node-bench.js` and `scatter-node-bench.js` tools run explicit
`node:bench` files without changing the existing benchmark framework or its
tools. Each repeated observation for a benchmark identity is collected by a
separate process invocation with one measured sample. Benchmarks declared in
the same file still execute serially in that process and can share JIT, garbage
collector, heap, and cache state. This differs from legacy configuration-level
process isolation and must be considered when comparing the frameworks.

Compare two binaries and analyze the compatible CSV using `compare.R`:

```console
./node benchmark/compare-node-bench.js \
  --old ./node-main --new ./node-pr --runs 30 -- \
  benchmark/crypto/_create-hash.node-bench.js > compare-node-bench.csv
Rscript benchmark/compare.R < compare-node-bench.csv
```

Pass `--analyze` to run the same Welch analysis inline. `--max-regression N`
implies `--analyze` and makes the command fail only when the Holm-Bonferroni
adjusted p-value is below 0.05 and the full 95% confidence interval is worse
than `-N%`. Requiring both conditions prevents a noisy point estimate from
failing a regression gate.

```console
./node benchmark/compare-node-bench.js \
  --old ./node-main --new ./node-pr --runs 30 \
  --max-regression 5 -- benchmark/crypto/_create-hash.node-bench.js
```

Collect parameter data for the parallel buffer benchmark and plot it using
`scatter.R`:

```console
./node benchmark/scatter-node-bench.js --node ./node --runs 30 -- \
  benchmark/buffers/_buffer-compare-offset.node-bench.js \
  > scatter-node-bench.csv
Rscript benchmark/scatter.R --xaxis size --category method \
  --plot scatter-node-bench.png < scatter-node-bench.csv
```

Pass `--analyze` with an x-axis parameter to summarize the samples without R.
The output includes mean and median confidence intervals, skew warnings, an
optional bar chart, and Mann-Whitney U and Cliff's delta comparisons between
consecutive x-axis values. Use `--category` for a second grouping parameter and
`--no-chart` to omit the chart.

Because configurations in one file share a process, inline analysis averages
aggregated configurations into one value per outer process. Consecutive
x-axis comparisons use alternating, disjoint process sets so the unpaired
Mann-Whitney test does not treat correlated values as independent samples.

```console
./node benchmark/scatter-node-bench.js --runs 30 --analyze \
  --xaxis size --category method -- \
  benchmark/buffers/_buffer-compare-offset.node-bench.js
```

A file passed to `scatter-node-bench.js` must use one logical benchmark name.
Parameter values distinguish its configurations. The tool rejects unstable
identities and names or parameters that would merge unrelated CSV groups.

The underscore-prefixed benchmark files are parallel ports used to compare the
measurement frameworks. Legacy discovery ignores them, so the original files
remain the source benchmarks for `run.js`, `compare.js`, and `scatter.js`. The
ports use the platform-specific original relative filename as their benchmark
name and preserve parameter column names to keep CSV grouping compatible. For
a direct framework comparison, collect the same number of runs from an
original benchmark with `scatter.js` and from its port with
`scatter-node-bench.js`, then compare their rate distributions.

## Common API

The common.js module is used by benchmarks for consistency across repeated
tasks. It has a number of helpful functions and properties to help with
writing benchmarks.

### `createBenchmark(fn, configs[, options])`

See [the guide on writing benchmarks](../doc/contributing/writing-and-running-benchmarks.md#basics-of-a-benchmark).

### `default_http_benchmarker`

The default benchmarker used to run HTTP benchmarks.
See [the guide on writing HTTP benchmarks](../doc/contributing/writing-and-running-benchmarks.md#creating-an-http-benchmark).

### `PORT`

The default port used to run HTTP benchmarks.
See [the guide on writing HTTP benchmarks](../doc/contributing/writing-and-running-benchmarks.md#creating-an-http-benchmark).

### `sendResult(data)`

Used in special benchmarks that can't use `createBenchmark` and the object
it returns to accomplish what they need. This function reports timing
data to the parent process (usually created by running `compare.js`, `run.js` or
`scatter.js`).
