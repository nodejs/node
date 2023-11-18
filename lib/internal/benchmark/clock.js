'use strict';

const {
  FunctionPrototypeBind,
  MathMax,
  NumberPrototypeToFixed,
  Symbol,
  Number,
  globalThis,
} = primordials;

const {
  codes: { ERR_INVALID_STATE },
} = require('internal/errors');
const { validateInteger, validateNumber } = require('internal/validators');

let debugBench = require('internal/util/debuglog').debuglog('benchmark', (fn) => {
  debugBench = fn;
});

const kUnmanagedTimerResult = Symbol('kUnmanagedTimerResult');

// If the smallest time measurement is 1ns
// the minimum resolution of this timer is 0.5
const MIN_RESOLUTION = 0.5;

class Timer {
  constructor() {
    this.now = process.hrtime.bigint;
  }

  get scale() {
    return 1e9;
  }

  get resolution() {
    return 1 / 1e9;
  }

  /**
   * @param {number} timeInNs
   * @returns {string}
   */
  format(timeInNs) {
    validateNumber(timeInNs, 'timeInNs', 0);

    if (timeInNs > 1e9) {
      return `${NumberPrototypeToFixed(timeInNs / 1e9, 2)}s`;
    }

    if (timeInNs > 1e6) {
      return `${NumberPrototypeToFixed(timeInNs / 1e6, 2)}ms`;
    }

    if (timeInNs > 1e3) {
      return `${NumberPrototypeToFixed(timeInNs / 1e3, 2)}us`;
    }

    if (timeInNs > 1e2) {
      return `${NumberPrototypeToFixed(timeInNs, 0)}ns`;
    }

    return `${NumberPrototypeToFixed(timeInNs, 2)}ns`;
  }
}

const timer = new Timer();

class ManagedTimer {
  /**
   * @type {bigint}
   */
  #start;
  /**
   * @type {bigint}
   */
  #end;
  /**
   * @type {number}
   */
  #iterations;
  /**
   * @type {number}
   */
  #recommendedCount;

  /**
   * @param {number} recommendedCount
   */
  constructor(recommendedCount) {
    this.#recommendedCount = recommendedCount;
  }

  /**
   * Returns the recommended value to be used to benchmark your code
   * @returns {number}
   */
  get count() {
    return this.#recommendedCount;
  }

  /**
   * Starts the timer
   */
  start() {
    this.#start = timer.now();
  }

  /**
   * Stops the timer
   * @param {number} [iterations=1] The amount of iterations that run
   */
  end(iterations = 1) {
    this.#end = timer.now();

    validateInteger(iterations, 'iterations', 1);

    this.#iterations = iterations;
  }

  [kUnmanagedTimerResult]() {
    if (this.#start === undefined)
      throw new ERR_INVALID_STATE('You forgot to call .start()');

    if (this.#end === undefined)
      throw new ERR_INVALID_STATE('You forgot to call .end(count)');

    return [Number(this.#end - this.#start), this.#iterations];
  }
}

function createRunUnmanagedBenchmark(awaitOrEmpty) {
  return `
const startedAt = timer.now();

for (let i = 0; i < count; i++)
  ${awaitOrEmpty}bench.fn();

const duration = Number(timer.now() - startedAt);
return [duration, count];
`;
}

function createRunManagedBenchmark(awaitOrEmpty) {
  return `
${awaitOrEmpty}bench.fn(timer);

return timer[kUnmanagedTimerResult]();
  `;
}

const AsyncFunction = async function() { }.constructor;
const SyncFunction = function() { }.constructor;

function createRunner(bench, recommendedCount) {
  const isAsync = bench.fn.constructor === AsyncFunction;
  const hasArg = bench.fn.length >= 1;

  if (bench.fn.length > 1) {
    process.emitWarning(`The benchmark "${bench.name}" function should not have more than 1 argument.`);
  }

  const compiledFnStringFactory = hasArg ? createRunManagedBenchmark : createRunUnmanagedBenchmark;
  const compiledFnString = compiledFnStringFactory(isAsync ? 'await ' : '');
  const createFnPrototype = isAsync ? AsyncFunction : SyncFunction;
  const compiledFn = createFnPrototype('bench', 'timer', 'count', 'kUnmanagedTimerResult', compiledFnString);

  const selectedTimer = hasArg ? new ManagedTimer(recommendedCount) : timer;

  const runner = FunctionPrototypeBind(
    compiledFn, globalThis, bench, selectedTimer, recommendedCount, kUnmanagedTimerResult);

  debugBench(`Created compiled benchmark, hasArg=${hasArg}, isAsync=${isAsync}, recommendedCount=${recommendedCount}`);

  return runner;
}

async function clockBenchmark(bench, recommendedCount) {
  const runner = createRunner(bench, recommendedCount);
  const result = await runner();

  // Just to avoid issues with empty fn
  result[0] = MathMax(MIN_RESOLUTION, result[0]);

  debugBench(`Took ${timer.format(result[0])} to execute ${result[1]} iterations`);

  return result;
}

module.exports = {
  clockBenchmark,
  debugBench,
  timer,
  MIN_RESOLUTION,
};
