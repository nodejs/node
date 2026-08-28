# Benchmark runner

<!--introduced_in=REPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1.0 - Early Development

<!-- source_link=lib/bench.js -->

The `node:bench` module supports defining and running JavaScript benchmarks in
the current process. To access it:

```mjs
import { bench, suite } from 'node:bench';
```

```cjs
const { bench, suite } = require('node:bench');
```

This module is only available under the `node:` scheme.

## Example benchmark

Save the following as `benchmark.mjs`:

```mjs
import { bench, suite } from 'node:bench';

suite('URL', () => {
  const input = 'https://example.com/a?b=c';

  bench('construct', {
    samples: 30,
    params: { input: 'short' },
  }, (b) => {
    const operations = 10_000;

    b.start();
    for (let i = 0; i < operations; i++) {
      new URL(input);
    }
    b.end(operations);
  });
});
```

Run the benchmark from the command line:

```console
node --bench benchmark.mjs
```

Benchmarks are executed serially in declaration order. Declared benchmarks are
scheduled automatically. Call `run()` during the same turn as the declarations
to consume the event stream or configure filtering.
If an automatically scheduled run fails and `run()` was not called, the process
exit code is set to `1`.

## Measurement model

Each warmup and measured sample invokes the benchmark function once with a
fresh {BenchContext}. The function must either call `context.start()` and
`context.end(operations)` exactly once, or call `context.record(sample)` exactly
once to provide an externally measured sample. Setup before `start()` and
cleanup after `end()` are outside the measured region. Promise-returning
functions are awaited.

By default, an event loop turn occurs between sample invocations. An embedded
runner can disable this using `yieldBetweenSamples`. The runner executes
benchmarks serially, but it does not provide process isolation. Other work in
the process, JIT compilation, garbage collection, CPU frequency changes, and
system load can all affect results. Keep raw samples when comparing results and
investigate noisy or skewed distributions rather than treating a confidence
interval as a pass/fail threshold.

Calling `context.done()` during a measured sample completes the benchmark after
that sample. This allows a higher-level tool to treat `samples` as a maximum and
implement a dynamic sampling policy.

## Reusable runners

The module-level declaration functions use a shared runner and schedule it
automatically. Higher-level tools can create isolated, explicitly started
runners instead:

```mjs
import { createRunner } from 'node:bench';

const runner = createRunner({ yieldBetweenSamples: false });

runner.bench('example', { samples: 100 }, (b) => {
  const operations = chooseOperationCount();
  b.start();
  runOperations(operations);
  const sample = b.end(operations);

  if (hasEnoughData(sample)) b.done();
});

for await (const record of runner.run()) {
  // Consume structured benchmark records.
}
```

Each runner has independent declarations, hooks, filtering, and output. Unlike
the module-level declarations, creating a benchmark on an explicit runner does
not schedule execution. This allows packages to collect declarations and start
them later. Calling the explicit runner's `run()` function prevents additional
declarations and a second call to `run()` is an error.

## Command-line runner

The `--bench` flag runs one or more explicit benchmark files or glob patterns:

```console
node --bench benchmark.mjs
node --bench --bench-reporter=json 'benchmarks/**/*.js'
```

Files are sorted and executed serially. The default
`--bench-isolation=process` mode runs each file in a separate child process and
emits one aggregate summary. Structured events are transferred to the parent
without JSON conversion, preserving BigInt durations, errors, and parameter
values. Child writes to stdout and stderr are emitted as diagnostic records so
they do not corrupt reporter output.

`--bench-isolation=none` imports all files into the runner process. This mode
has lower startup overhead, but module, heap, and process state carry between
files, and user writes share stdout and stderr with reporters.

Benchmark files passed to `--bench` should declare benchmarks but must not call
`run()`. The CLI supports `--bench-name-pattern`, `--bench-samples`,
`--bench-warmup`, `--bench-reporter`, and `--bench-reporter-destination`. See
the [command-line options documentation][] for details.

Preload modules passed through `--require` or `--import` should not declare
benchmarks. Such declarations are not associated with an entry file and have
an `entryFile` value of `null`. Their `fileRunId` identifies the runner or child
execution in which they occurred. With process isolation, a preload is evaluated
and its declarations run once for every benchmark child process.

## Benchmark reporters

The built-in reporters are available from the scheme-only
`node:bench/reporters` module:

```mjs
import { json, spec } from 'node:bench/reporters';
```

```cjs
const { json, spec } = require('node:bench/reporters');
```

Reporter values can be passed directly to `stream.compose()`:

```mjs
import { bench, run } from 'node:bench';
import { spec } from 'node:bench/reporters';
import process from 'node:process';

bench('example', (b) => {
  b.start();
  doWork();
  b.end(1);
});

run().compose(spec).pipe(process.stdout);
```

The `spec` reporter buffers results and outputs a concise table containing the
sample count, mean rate, 95% confidence interval for the mean, median rate, and
warnings. A coefficient of variation above 5% is reported as `noisy`, and an
absolute skewness above 1 is reported as `skewed`. The exact human-readable
format is subject to change.

The `json` reporter emits every lifecycle record as newline-delimited JSON.
BigInt values, including `duration_ns`, are encoded as decimal strings. Errors
are represented using their `name`, `message`, `stack`, `code`, `cause`, and
`errors` properties. As required by JSON, non-finite numbers are encoded as
`null`.

Custom reporters use the same composition contract. They can be transforms or
functions accepted by `stream.compose()`. The composed readable can be piped to
any writable destination:

```mjs
import { run } from 'node:bench';
import process from 'node:process';

async function* names(source) {
  for await (const { type, data } of source) {
    if (type === 'bench:complete') {
      yield `${data.name}\n`;
    }
  }
}

run().compose(names).pipe(process.stdout);
```

## `createRunner([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `yieldBetweenSamples` {boolean} Schedule an event loop turn between sample
    callbacks. Disabling this also prevents timer-based abort signals from
    firing between synchronous callbacks. Benchmark timeouts continue to be
    checked against a monotonic deadline. **Default:** `true`.
* Returns: {Object} An isolated benchmark runner with bound `after`, `afterEach`,
  `before`, `beforeEach`, `bench`, `describe`, `run`, and `suite` functions.

Creates an explicitly started benchmark runner. Declarations made through one
runner do not interact with declarations made through another runner or through
the module-level functions. Call the returned `run()` function to start the
runner and obtain its {BenchmarksStream}.

Each runner can be started once. Its `run()` function accepts the same options
as the module-level [`run()`][]. `run({ yieldBetweenSamples })` overrides the
value passed to `createRunner()`.

## `bench([name][, options], fn)`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The benchmark name. **Default:** The `name` property of `fn`,
  or `'<anonymous>'` when `fn` has no name.
* `options` {Object}
  * `only` {boolean} When any benchmark or containing suite has `only` set,
    benchmarks without `only` in their hierarchy are skipped. **Default:**
    `false`.
  * `params` {Object} String, finite number, or boolean metadata identifying
    this benchmark configuration. Parameter keys are sorted when constructing
    the stable benchmark identity. **Default:** An empty object.
  * `samples` {number} The maximum number of measured callback invocations.
    Must be a positive 32-bit unsigned integer. The benchmark may finish earlier
    by calling `context.done()`. **Default:** `30`.
  * `signal` {AbortSignal} Allows aborting this benchmark.
  * `skip` {boolean|string} If truthy, the benchmark is skipped. A string is
    included in the result as the skip reason. **Default:** `false`.
  * `tags` {string\[]} Labels associated with the benchmark. Tags are
    lowercased, deduplicated, and inherited from containing suites by union.
    **Default:** `[]`.
  * `timeout` {number} The number of milliseconds after which the benchmark
    fails. **Default:** `Infinity`.
  * `warmup` {number} The number of unreported callback invocations before
    measured samples. Must be a 32-bit unsigned integer. **Default:** `0`.
* `fn` {Function|AsyncFunction} The benchmark function. It receives a
  {BenchContext}.
* Returns: {Promise} Fulfilled with the benchmark result after a top-level
  benchmark finishes, or with `undefined` immediately when declared in a
  suite.

Warmup invocations use the same callback and timing contract as measured
samples, but their samples are discarded. An exception, rejection, timeout,
abort, missing timing call, or duplicate timing call stops the current
benchmark. Later benchmarks continue to run.

A timeout or abort cannot interrupt synchronous JavaScript and does not forcibly
cancel asynchronous work that ignores `context.signal`.

The `benchId` is based on the declaration source file, hierarchical suite and
benchmark names, and canonicalized parameters. It is stable for repeated runs
from the same source location, but the embedded source value is not normalized
across checkout roots, module formats, operating systems, or path casing.

Execution scope is represented separately. A `runId` identifies one logical
run, while `fileRunId` identifies a file runner or child execution within that
run. The `entryFile` field records which entry-file import caused a declaration
and is `null` for declarations made by preload modules.
The same `benchId` can therefore occur under multiple `fileRunId` values when
entry files use a shared declaration helper. Declaring the same `benchId` more
than once within one file execution scope reports an error rather than merging
the samples.

### `bench.skip([name][, options], fn)`

<!-- YAML
added: REPLACEME
-->

Shorthand for `bench(name, { ...options, skip: true }, fn)`.

### `bench.only([name][, options], fn)`

<!-- YAML
added: REPLACEME
-->

Shorthand for `bench(name, { ...options, only: true }, fn)`.

## `suite([name][, options], fn)`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The suite name. **Default:** The `name` property of `fn`, or
  `'<anonymous>'` when `fn` has no name.
* `options` {Object}
  * `only` {boolean} Selects all benchmarks nested in this suite. **Default:**
    `false`.
  * `skip` {boolean|string} Skips all benchmarks nested in this suite.
    **Default:** `false`.
  * `tags` {string\[]} Labels inherited by nested suites and benchmarks.
    **Default:** `[]`.
* `fn` {Function|AsyncFunction} A function that declares nested suites,
  benchmarks, and hooks.
* Returns: {Promise} Fulfilled when a top-level suite finishes, or with
  `undefined` immediately when declared in another suite.

Suite functions run while declarations are collected. Promise-returning suite
functions are awaited before benchmark execution begins.

## `describe([name][, options], fn)`

<!-- YAML
added: REPLACEME
-->

Alias for `suite()`.

## `before(fn)`

<!-- YAML
added: REPLACEME
-->

* `fn` {Function|AsyncFunction} The hook function.

Registers a hook that runs once before the benchmarks in the current suite.

## `after(fn)`

<!-- YAML
added: REPLACEME
-->

* `fn` {Function|AsyncFunction} The hook function.

Registers a hook that runs once after the benchmarks in the current suite.

## `beforeEach(fn)`

<!-- YAML
added: REPLACEME
-->

* `fn` {Function|AsyncFunction} The hook function. It receives an object with
  the benchmark's `name`, `params`, and `signal`.

Registers a hook that runs once before each complete logical benchmark in the
current suite. It does not run before every sample. Per-sample setup belongs in
the benchmark function before `context.start()` or `context.record()`.

## `afterEach(fn)`

<!-- YAML
added: REPLACEME
-->

* `fn` {Function|AsyncFunction} The hook function. It receives an object with
  the benchmark's `name`, `params`, and `signal`.

Registers a hook that runs once after each complete logical benchmark in the
current suite. It does not run after every sample. Per-sample cleanup belongs
in the benchmark function after `context.end()` or `context.record()`.

## `run([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object}
  * `namePattern` {string|RegExp} Only runs benchmarks whose full hierarchical
    name matches the pattern. String values are interpreted as JavaScript
    regular expressions.
  * `samples` {number} Overrides the maximum number of measured callback
    invocations for every benchmark. Must be a positive 32-bit unsigned integer.
  * `signal` {AbortSignal} Allows aborting in-progress benchmark execution.
  * `warmup` {number} Overrides the number of unreported warmup callback
    invocations for every benchmark. Must be a 32-bit unsigned integer.
  * `yieldBetweenSamples` {boolean} Schedule an event loop turn between sample
    callbacks. **Default:** `true`, or the value passed to `createRunner()` for
    an explicit runner.
* Returns: {BenchmarksStream}

Returns the object-mode event stream for the in-process benchmark run. Call
`run()` during the same turn in which benchmarks are declared, before automatic
execution begins. Calling `run()` is optional when the returned stream is not
needed. An explicit runner created by `createRunner()` does not run
automatically, so its `run()` function may be called later.

```mjs
import { bench, run } from 'node:bench';

bench('example', { samples: 3 }, (b) => {
  b.start();
  doWork();
  b.end(1);
});

for await (const { type, data } of run()) {
  if (type === 'bench:complete' && data.error === undefined) {
    console.log(data.name, data.summary.mean);
  }
}
```

## Class: `BenchContext`

An instance of `BenchContext` is passed to every benchmark invocation. A new
instance is created for every warmup and measured sample.

### `context.index`

<!-- YAML
added: REPLACEME
-->

* {number}

The zero-based invocation index within the current `context.phase`. Warmup and
measured samples have separate index sequences.

### `context.name`

<!-- YAML
added: REPLACEME
-->

* {string}

The benchmark name.

### `context.params`

<!-- YAML
added: REPLACEME
-->

* {Object}

The benchmark's canonicalized parameter metadata.

### `context.phase`

<!-- YAML
added: REPLACEME
-->

* {string}

The current sample phase. It is `'warmup'` for an unreported warmup invocation
and `'measurement'` for a measured invocation.

### `context.signal`

<!-- YAML
added: REPLACEME
-->

* {AbortSignal}

An abort signal that is triggered when the benchmark is aborted, times out, or
finishes.

### `context.start()`

<!-- YAML
added: REPLACEME
-->

Starts the measured region using `process.hrtime.bigint()`. Calling `start()`
more than once is an error.

### `context.end(operations[, options])`

<!-- YAML
added: REPLACEME
-->

* `operations` {number} The number of completed operations. Must be a positive
  safe integer.
* `options` {Object}
  * `detail` {any} Additional structured-cloneable sample data. With CLI process
    isolation, it must also be supported by advanced child process
    serialization.
* Returns: {Object} The sample's `operations`, `duration_ns`, computed `rate`,
  and optional cloned `detail`.

Ends the measured region. The end timestamp is captured before `operations` is
validated. Calling `end()` before `start()`, calling it more than once, or
recording a zero-duration sample is an error. When provided, `detail` is cloned
after the end timestamp is captured, so cloning time is outside the measured
region.

### `context.record(sample)`

<!-- YAML
added: REPLACEME
-->

* `sample` {Object}
  * `operations` {number} The number of completed operations. Must be a positive
    safe integer.
  * `duration_ns` {bigint} An externally measured positive duration in
    nanoseconds no greater than `Number.MAX_SAFE_INTEGER`.
  * `detail` {any} Additional structured-cloneable sample data. With CLI process
    isolation, it must also be supported by advanced child process
    serialization.
* Returns: {Object} The normalized sample, including its computed `rate` and
  optional cloned `detail`.

Records a measurement made by another clock or execution environment. This is
useful when a higher-level tool measures work in a worker and needs to exclude
message transport from the duration. `record()` is mutually exclusive with
`start()` and `end()` within one callback and must be called exactly once.

### `context.done()`

<!-- YAML
added: REPLACEME
-->

Requests successful benchmark completion after the current measured sample.
The callback must still call either `start()` and `end()`, or `record()`.
Calling `done()` during a warmup invocation is an error. The configured
`samples` value remains the maximum number of measured invocations if `done()`
is not called.

## Class: `BenchmarksStream`

`BenchmarksStream` is an object-mode {stream.Readable}. Each lifecycle record is
both emitted as a named event and made available on the stream as
`{ type, data }`.

The events are emitted in execution order:

* `'bench:start'`
* `'bench:sample'`
* `'bench:complete'`
* `'bench:diagnostic'`
* `'bench:summary'`

Every benchmark-scoped event contains `runId`, `fileRunId`, `entryFile`,
`benchId`, `parentId`, and `namePath`. `runId` and `fileRunId` are opaque and
change between runs. `entryFile` identifies the top-level benchmark file whose
loading caused the declaration, while `file` identifies the source location of
the declaration itself. `parentId` is based on the containing suite's source
file and hierarchical name path.

`'bench:complete'` data contains a [benchmark result][]. A failed result has an
additional `error` property and may contain samples recorded before the error.
A skipped result has an additional `skip` property and an empty `samples`
array. `'bench:diagnostic'` reports suite and hook errors. `'bench:summary'`
contains overall `runId`, `fileRunId`, `entryFile`, `success`, `counts`,
`duration_ns`, and `file` properties. `fileRunId`, `entryFile`, and `file` are
{string|null}; they are `null` when the summary aggregates multiple files.

## Sample result

Each measured sample has the following properties:

* `operations` {number} The positive operation count passed to
  `context.end()` or `context.record()`.
* `duration_ns` {bigint} The measured duration in nanoseconds.
* `rate` {number} Operations per second.
* `detail` {any} The optional cloned sample detail.

## Benchmark result

A completed benchmark result contains:

* `runId` {string} The opaque logical run identity.
* `fileRunId` {string} The opaque file runner or child execution identity.
* `entryFile` {string|null} The top-level file that caused this declaration.
* `benchId` {string} The stable declaration identity within the same source
  layout.
* `parentId` {string|null} The stable containing suite identity.
* `name` {string} The benchmark name.
* `namePath` {string\[]} The hierarchical suite and benchmark names.
* `file` {string} The declaration source file.
* `line` {number} The source line.
* `column` {number} The source column.
* `tags` {string\[]} The inherited canonical tags.
* `params` {Object} The canonical parameter metadata.
* `samples` {Object\[]} The exact measured samples.
* `summary` {Object}
  * `mean` {number} The arithmetic mean of per-sample rates.
  * `median` {number} The median per-sample rate.
  * `min` {number} The minimum per-sample rate.
  * `max` {number} The maximum per-sample rate.
  * `stddev` {number} The population standard deviation of rates.
  * `coefficientOfVariation` {number} `stddev / mean`.
  * `confidenceInterval` {Object} The 95% Student's t confidence interval for
    the mean rate, with `lower` and `upper` properties.
  * `medianConfidenceInterval` {Object} The 95% nonparametric confidence
    interval for the median rate, with `lower` and `upper` properties.
  * `skewness` {number} The skewness of the scaled rate histogram.

[`run()`]: #runoptions
[benchmark result]: #benchmark-result
[command-line options documentation]: cli.md#--bench
