'use strict';

const { ArrayPrototypePush } = primordials;
const { debugBench, timer } = require('internal/benchmark/clock');
const { reportConsoleBench } = require('internal/benchmark/report');
const { maxTime, minTime, runBenchmark, getInitialIterations } = require('internal/benchmark/lifecycle');
const { validateNumber, validateString, validateFunction, validateObject } = require('internal/validators');

class Benchmark {
  #name;
  #fn;
  #minTime;
  #maxTime;

  constructor(name, fn, minTime, maxTime) {
    this.#name = name;
    this.#fn = fn;
    this.#minTime = minTime;
    this.#maxTime = maxTime;
  }

  get name() {
    return this.#name;
  }

  get fn() {
    return this.#fn;
  }

  get minTime() {
    return this.#minTime;
  }

  get maxTime() {
    return this.#maxTime;
  }
}

class Suite {
  #benchmarks;
  #reporter;

  constructor(options) {
    this.#benchmarks = [];

    if (typeof options !== 'undefined') {
      validateObject(options, 'options');

      options.reporter ??= reportConsoleBench;

      validateFunction(options.reporter, 'options.reporter');

      this.#reporter = options.reportConsoleBench;
    } else {
      this.#reporter = reportConsoleBench;
    }
  }

  add(name, options, fn) {
    validateString(name, 'name');

    let benchOptions;
    let benchFn;

    if (typeof options === 'object') {
      benchOptions = options;
      benchFn = fn;
    } else if (typeof options === 'function') {
      benchFn = options;
      benchOptions = fn;
    }

    validateFunction(benchFn, 'fn');

    if (typeof benchOptions !== 'undefined') {
      validateObject(benchOptions, 'options');
    }

    const benchMinTime = benchOptions?.minTime ?? minTime;
    const benchMaxTime = benchOptions?.maxTime ?? maxTime;

    validateNumber(benchMinTime, 'options.minTime', timer.resolution * 1e3);
    validateNumber(benchMaxTime, 'options.maxTime', benchMinTime);

    const benchmark = new Benchmark(name, benchFn, benchMinTime, benchMaxTime);

    ArrayPrototypePush(this.#benchmarks, benchmark);

    return this;
  }

  async run() {
    const results = new Array(this.#benchmarks.length);
    const initialIterations = new Array(this.#benchmarks.length);

    // We need to calculate the initial iterations first
    // because this helps to reduce noise/bias on the results,
    // when changing the order of the benchmarks also changes
    // the amount of ops/sec
    for (let i = 0; i < this.#benchmarks.length; i++) {
      initialIterations[i] = await getInitialIterations(this.#benchmarks[i]);
    }

    for (let i = 0; i < this.#benchmarks.length; i++) {
      const benchmark = this.#benchmarks[i];
      const initialIteration = initialIterations[i];

      debugBench(`Starting ${benchmark.name} with minTime=${benchmark.minTime}, maxTime=${benchmark.maxTime}`);

      const result = await runBenchmark(benchmark, initialIteration);
      results[i] = result;

      this.#reporter(benchmark, result);
    }

    return results;
  }
}

module.exports = {
  Suite,
};
