# Node.js core benchmark tests

This folder contains benchmark tests to measure the performance for certain
Node.js APIs.

## Prerequisites

Most of the http benchmarks require [`wrk`][wrk] and [`ab`][ab] (ApacheBench) being installed.
These may be available through your preferred package manager.

If they are not available:
- `wrk` may easily be built [from source][wrk] via `make`.
- `ab` is sometimes bundled in a package called `apache2-utils`.

[wrk]: https://github.com/wg/wrk
[ab]: http://httpd.apache.org/docs/2.2/programs/ab.html

## How to run tests

There are three ways to run benchmark tests:

### Run all tests of a given type

For example, buffers:

```bash
node benchmark/common.js buffers
```

The above command will find all scripts under `buffers` directory and require
each of them as a module. When a test script is required, it creates an instance
of `Benchmark` (a class defined in common.js). In the next tick, the `Benchmark`
constructor iterates through the configuration object property values and runs
the test function with each of the combined arguments in spawned processes. For
example, buffers/buffer-read.js has the following configuration:

```js
var bench = common.createBenchmark(main, {
    noAssert: [false, true],
    buffer: ['fast', 'slow'],
    type: ['UInt8', 'UInt16LE', 'UInt16BE',
        'UInt32LE', 'UInt32BE',
        'Int8', 'Int16LE', 'Int16BE',
        'Int32LE', 'Int32BE',
        'FloatLE', 'FloatBE',
        'DoubleLE', 'DoubleBE'],
        millions: [1]
});
```
The runner takes one item from each of the property array value to build a list
of arguments to run the main function. The main function will receive the conf
object as follows:

- first run:
```js
    {   noAssert: false,
        buffer: 'fast',
        type: 'UInt8',
        millions: 1
    }
```
- second run:
```js
    {
        noAssert: false,
        buffer: 'fast',
        type: 'UInt16LE',
        millions: 1
    }
```
...

In this case, the main function will run 2*2*14*1 = 56 times. The console output
looks like the following:

```
buffers//buffer-read.js
buffers/buffer-read.js noAssert=false buffer=fast type=UInt8 millions=1: 271.83
buffers/buffer-read.js noAssert=false buffer=fast type=UInt16LE millions=1: 239.43
buffers/buffer-read.js noAssert=false buffer=fast type=UInt16BE millions=1: 244.57
...
```

The last number is the rate of operations. Higher is better.

### Run an individual test

For example, buffer-slice.js:

```bash
node benchmark/buffers/buffer-read.js
```
The output:
```
buffers/buffer-read.js noAssert=false buffer=fast type=UInt8 millions=1: 246.79
buffers/buffer-read.js noAssert=false buffer=fast type=UInt16LE millions=1: 240.11
buffers/buffer-read.js noAssert=false buffer=fast type=UInt16BE millions=1: 245.91
...
```

### Run tests with options

This example will run only the first type of url test, with one iteration.
(Note: benchmarks require __many__ iterations to be statistically accurate.)


```bash
node benchmark/url/url-parse.js type=one n=1
```
Output:
```
url/url-parse.js type=one n=1: 1663.74402
```

## How to write a benchmark test

The benchmark tests are grouped by types. Each type corresponds to a subdirectory,
such as `arrays`, `buffers`, or `fs`.

Let's add a benchmark test for Buffer.slice function. We first create a file
buffers/buffer-slice.js.

### The code snippet

```js
var common = require('../common.js'); // Load the test runner

var SlowBuffer = require('buffer').SlowBuffer;

// Create a benchmark test for function `main` and the configuration variants
var bench = common.createBenchmark(main, {
  type: ['fast', 'slow'], // Two types of buffer
  n: [512] // Number of times (each unit is 1024) to call the slice API
});

function main(conf) {
  // Read the parameters from the configuration
  var n = +conf.n;
  var b = conf.type === 'fast' ? buf : slowBuf;
  bench.start(); // Start benchmarking
  for (var i = 0; i < n * 1024; i++) {
    // Add your test here
    b.slice(10, 256);
  }
  bench.end(n); // End benchmarking
}
```
