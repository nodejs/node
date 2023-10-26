# Benchmark

<!--introduced_in=REPLACEME-->

> Stability: 1.1 - Active Development

<!-- source_link=lib/benchmark.js -->

The `node:benchmark` module gives the ability to measure
performance of JavaScript code. To access it:

```mjs
import benchmark from 'node:benchmark';
```

```cjs
const benchmark = require('node:benchmark');
```

This module is only available under the `node:` scheme. The following will not
work:

```mjs
import benchmark from 'benchmark';
```

```cjs
const benchmark = require('benchmark');
```

The following example illustrates how benchmarks are written using the
`benchmark` module.

```mjs
import { Suite } from 'node:benchmark';

const suite = new Suite();

suite.add('Using delete to remove property from object', function() {
  const data = { x: 1, y: 2, z: 3 };
  delete data.y;

  data.x;
  data.y;
  data.z;
});

suite.run();
```

```cjs
const { Suite } = require('node:benchmark');

const suite = new Suite();

suite.add('Using delete to remove property from object', function() {
  const data = { x: 1, y: 2, z: 3 };
  delete data.y;

  data.x;
  data.y;
  data.z;
});

suite.run();
```

```console
$ node my-benchmark.js
(node:14165) ExperimentalWarning: The benchmark module is an experimental feature and might change at any time
(Use `node --trace-warnings ...` to show where the warning was created)
Using delete property x 5,853,505 ops/sec ± 0.01% (10 runs sampled)     min..max=(169ns ... 171ns) p75=170ns p99=171ns
```

## Class: `Suite`

> Stability: 1.1 Active Development

<!-- YAML
added: REPLACEME
-->

An `Suite` is responsible for managing and executing
benchmark functions. It provides two methods: `add()` and `run()`.

### `new Suite([options])`

<!-- YAML
added: REPLACEME
-->

* `options` {Object} Configuration options for the suite. The following
  properties are supported:
  * `reporter` {Function} Callback function with results to be called after
    benchmark is concluded. The callback function should receive two arguments:
    `suite` - A {Suite} object and
    `result` - A object containing three properties:
    `opsSec` {string}, `iterations {Number}`, `histogram` {Histogram} instance.

If no `reporter` is provided, the results will printed to the console.

```mjs
import { Suite } from 'node:benchmark';
const suite = new Suite();
```

```cjs
const { Suite } = require('node:benchmark');
const suite = new Suite();
```

### `suite.add(name[, options], fn)`

<!-- YAML
added: REPLACEME
-->

* `name` {string} The name of the benchmark, which is displayed when reporting
  benchmark results.
* `options` {Object} Configuration options for the benchmark. The following
  properties are supported:
  * `minTime` {number} The minimum time a benchmark can run.
    **Default:** `0.05` seconds.
  * `maxTime` {number} The maximum time a benchmark can run.
    **Default:** `0.5` seconds.
* `fn` {Function|AsyncFunction}
* Returns: {Suite}

This method stores the benchmark of a given function (`fn`).
The `fn` parameter can be either an asynchronous (`async function () {}`) or
a synchronous (`function () {}`) function.

```console
$ node my-benchmark.js
(node:14165) ExperimentalWarning: The benchmark module is an experimental feature and might change at any time
(Use `node --trace-warnings ...` to show where the warning was created)
Using delete property x 5,853,505 ops/sec ± 0.01% (10 runs sampled)     min..max=(169ns ... 171ns) p75=170ns p99=171ns
```

### `suite.run()`

* Returns: `{Promise<Array<Object>>}`
  * `opsSec` {number} The amount of operations per second
  * `iterations` {number} The amount executions of `fn`
  * `histogram` {Histogram} Histogram object used to record benchmark iterations

<!-- YAML
added: REPLACEME
-->

The purpose of the run method is to run all the benchmarks that have been
added to the suite using the [`suite.add()`][] function.
By calling the run method, you can easily trigger the execution of all
the stored benchmarks and obtain the corresponding results.

### Using custom reporter

You can customize the data reporting by passing an function to the `reporter` argument while creating your `Suite`:

```mjs
import { Suite } from 'node:benchmark';

function reporter(bench, result) {
  console.log(`Benchmark: ${bench.name} - ${result.opsSec} ops/sec`);
}

const suite = new Suite({ reporter });

suite.add('Using delete to remove property from object', () => {
  const data = { x: 1, y: 2, z: 3 };
  delete data.y;

  data.x;
  data.y;
  data.z;
});

suite.run();
```

```cjs
const { Suite } = require('node:benchmark');

function reporter(bench, result) {
  console.log(`Benchmark: ${bench.name} - ${result.opsSec} ops/sec`);
}

const suite = new Suite({ reporter });

suite.add('Using delete to remove property from object', () => {
  const data = { x: 1, y: 2, z: 3 };
  delete data.y;

  data.x;
  data.y;
  data.z;
});

suite.run();
```

```console
$ node my-benchmark.js
Benchmark: Using delete to remove property from object - 6032212 ops/sec
```

### Setup and Teardown

The benchmark function has a special handling when you pass an argument,
for example:

```cjs
const { Suite } = require('node:benchmark');
const { readFileSync, writeFileSync, rmSync } = require('node:fs');

const suite = new Suite();

suite.add('readFileSync', (timer) => {
  const randomFile = Date.now();
  const filePath = `./${randomFile}.txt`;
  writeFileSync(filePath, Math.random().toString());

  timer.start();
  readFileSync(filePath, 'utf8');
  timer.end();

  rmSync(filePath);
}).run();
```

In this way, you can control when the `timer` will start
and also when the `timer` will stop.

In the timer, we also give you a property `count`
that will tell you how much iterations
you should run your function to achieve the `benchmark.minTime`,
see the following example:

```mjs
import { Suite } from 'node:benchmark';
import { readFileSync, writeFileSync, rmSync } from 'node:fs';

const suite = new Suite();

suite.add('readFileSync', (timer) => {
  const randomFile = Date.now();
  const filePath = `./${randomFile}.txt`;
  writeFileSync(filePath, Math.random().toString());

  timer.start();
  for (let i = 0; i < timer.count; i++)
    readFileSync(filePath, 'utf8');
  // You must send to the `.end` function the amount of
  // times you executed the function, by default,
  // the end will be called with value 1.
  timer.end(timer.count);

  rmSync(filePath);
});

suite.run();
```

```cjs
const { Suite } = require('node:benchmark');
const { readFileSync, writeFileSync, rmSync } = require('node:fs');

const suite = new Suite();

suite.add('readFileSync', (timer) => {
  const randomFile = Date.now();
  const filePath = `./${randomFile}.txt`;
  writeFileSync(filePath, Math.random().toString());

  timer.start();
  for (let i = 0; i < timer.count; i++)
    readFileSync(filePath, 'utf8');
  // You must send to the `.end` function the amount of
  // times you executed the function, by default,
  // the end will be called with value 1.
  timer.end(timer.count);

  rmSync(filePath);
});

suite.run();
```

Once your function has at least one argument,
you must call `.start` and `.end`, if you didn't,
it will throw the error [ERR\_BENCHMARK\_MISSING\_OPERATION](./errors.md#err_benchmark_missing_operation).

[`suite.add()`]: #suiteaddname-options-fn
