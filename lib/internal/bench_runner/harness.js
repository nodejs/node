'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeReverse,
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  Promise,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
  RegExp,
  RegExpPrototypeExec,
  SafeMap,
  SafePromiseRace,
  SymbolDispose,
} = primordials;
const { getCallerLocation } = internalBinding('util');
const { exitCodes: { kGenericUserError } } = internalBinding('errors');
const { AsyncLocalStorage } = require('async_hooks');
const { AbortController } = require('internal/abort_controller');
const {
  AbortError,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_OPERATION_FAILED,
  },
} = require('internal/errors');
const { addAbortListener } = require('internal/events/abort_listener');
const {
  kEmptyObject,
} = require('internal/util');
const { isRegExp } = require('internal/util/types');
const {
  validateAbortSignal,
  validateFunction,
  validateObject,
} = require('internal/validators');
const { queueMicrotask } = require('internal/process/task_queues');
const { clearTimeout, setImmediate, setTimeout } = require('timers');
const {
  Bench,
  BenchContext,
  Suite,
  normalizeArgs,
  summarizeSamples,
} = require('internal/bench_runner/benchmark');
const {
  BenchmarksStream,
} = require('internal/bench_runner/benchmarks_stream');

const { bigint: hrtime } = process.hrtime;
const kHookNames = ['after', 'afterEach', 'before', 'beforeEach'];

function eventLoopTurn() {
  return new Promise((resolve) => setImmediate(resolve));
}

function createAbortError(signal) {
  return new AbortError(undefined, { __proto__: null, cause: signal.reason });
}

class Harness {
  #buildPromises = [];
  #duplicateErrors = new SafeMap();
  #explicitRun = false;
  #hasOnly = false;
  #runPromise = null;
  #scheduled = false;
  #storage = new AsyncLocalStorage();

  constructor() {
    this.entryFile = process.argv?.[1];
    this.stream = new BenchmarksStream();
    this.state = 'collecting';
    this.namePattern = null;
    this.outerSignal = undefined;
    this.success = true;
    this.counts = {
      __proto__: null,
      completed: 0,
      failed: 0,
      skipped: 0,
      total: 0,
    };
    this.root = new Suite(
      this,
      null,
      '<root>',
      kEmptyObject,
      undefined,
      undefined,
      true,
    );
  }

  #ensureCollecting() {
    if (this.state === 'collecting' ||
        (this.state === 'building' &&
         this.#storage.getStore() instanceof Suite)) return;
    throw new ERR_INVALID_STATE(
      'benchmarks cannot be declared after execution has started');
  }

  #getParent() {
    const current = this.#storage.getStore();
    return current instanceof Suite ? current : this.root;
  }

  createBench(name, options, fn, overrides = kEmptyObject) {
    this.#ensureCollecting();
    const normalized = normalizeArgs('benchmark', name, options, fn);
    const parent = this.#getParent();
    const benchmark = new Bench(
      this,
      parent,
      normalized.name,
      { __proto__: null, ...normalized.options, ...overrides },
      normalized.fn,
      overrides.loc,
    );
    ArrayPrototypePush(parent.children, benchmark);
    this.#schedule();
    return parent.isRoot ? benchmark.completion.promise : PromiseResolve();
  }

  createSuite(name, options, fn, overrides = kEmptyObject) {
    this.#ensureCollecting();
    const normalized = normalizeArgs('suite', name, options, fn);
    const parent = this.#getParent();
    const suite = new Suite(
      this,
      parent,
      normalized.name,
      { __proto__: null, ...normalized.options, ...overrides },
      normalized.fn,
      overrides.loc,
    );
    ArrayPrototypePush(parent.children, suite);
    this.#buildSuite(suite);
    this.#schedule();
    return parent.isRoot ? suite.completion.promise : PromiseResolve();
  }

  createHook(name, fn, options = kEmptyObject) {
    this.#ensureCollecting();
    validateFunction(fn, 'hook function');
    validateObject(options, 'options');
    const parent = this.#getParent();
    ArrayPrototypePush(parent.hooks[name], {
      __proto__: null,
      fn,
      loc: getCallerLocation(),
    });
    this.#schedule();
  }

  #buildSuite(suite) {
    let result;
    try {
      result = suite.runInAsyncScope(() => this.#storage.run(
        suite,
        () => FunctionPrototypeCall(suite.fn),
      ));
    } catch (error) {
      suite.buildError = error;
      result = undefined;
    }

    suite.buildPromise = PromisePrototypeThen(
      PromiseResolve(result),
      undefined,
      (error) => {
        suite.buildError = error;
      },
    );
    ArrayPrototypePush(this.#buildPromises, suite.buildPromise);
  }

  configure(options = kEmptyObject) {
    validateObject(options, 'options');
    if (this.#runPromise !== null) {
      if (options !== kEmptyObject) {
        throw new ERR_INVALID_STATE('benchmark execution has already started');
      }
      return;
    }

    const { namePattern, signal } = options;
    if (namePattern !== undefined) {
      if (typeof namePattern === 'string') {
        this.namePattern = new RegExp(namePattern);
      } else if (isRegExp(namePattern)) {
        this.namePattern = namePattern;
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          'options.namePattern', ['string', 'RegExp'], namePattern);
      }
    }
    validateAbortSignal(signal, 'options.signal');
    this.outerSignal = signal;
    this.#explicitRun = true;
  }

  run(options = kEmptyObject) {
    this.configure(options);
    this.#schedule();
    return this.stream;
  }

  #schedule() {
    if (this.#scheduled) return;
    this.#scheduled = true;
    queueMicrotask(() => {
      if (this.#runPromise === null) {
        this.#runPromise = this.#execute();
        PromisePrototypeThen(this.#runPromise, undefined, (error) => {
          this.#diagnostic(error, undefined, 'error');
          this.#finish();
        });
      }
    });
  }

  async #waitForBuild() {
    for (let i = 0; i < this.#buildPromises.length; i++) {
      await this.#buildPromises[i];
    }
  }

  #walk(node, callback) {
    for (let i = 0; i < node.children.length; i++) {
      const child = node.children[i];
      callback(child);
      if (child instanceof Suite) this.#walk(child, callback);
    }
  }

  #prepare() {
    const identities = new SafeMap();
    this.#walk(this.root, (node) => {
      if (node.only) this.#hasOnly = true;
      if (!(node instanceof Bench)) return;

      this.counts.total++;
      const existing = identities.get(node.benchId);
      if (existing === undefined) {
        identities.set(node.benchId, node);
      } else {
        this.#duplicateErrors.set(node, new ERR_INVALID_STATE(
          `duplicate benchmark identity for "${node.fullName}"`));
      }
    });
  }

  #hasSelectedAncestor(benchmark) {
    for (let current = benchmark; current !== null; current = current.parent) {
      if (current.only) return true;
    }
    return false;
  }

  #getSkip(benchmark) {
    for (let current = benchmark; current !== null; current = current.parent) {
      if (current.skip !== undefined && current.skip !== false) {
        return current.skip;
      }
    }
    if (this.#hasOnly && !this.#hasSelectedAncestor(benchmark)) return 'only';
    if (this.namePattern !== null) {
      this.namePattern.lastIndex = 0;
      if (RegExpPrototypeExec(this.namePattern, benchmark.fullName) === null) {
        return 'name pattern';
      }
    }
    return null;
  }

  #suiteHasActiveBench(suite) {
    for (let i = 0; i < suite.children.length; i++) {
      const child = suite.children[i];
      if (child instanceof Suite) {
        if (this.#suiteHasActiveBench(child)) return true;
      } else if (this.#getSkip(child) === null) {
        return true;
      }
    }
    return false;
  }

  async #invoke(resource, store, fn, args) {
    const result = resource.runInAsyncScope(() => this.#storage.run(
      store,
      () => ReflectApply(fn, undefined, args),
    ));
    return PromiseResolve(result);
  }

  async #runHooks(suite, name, resource, store, context) {
    const hooks = suite.hooks[name];
    for (let i = 0; i < hooks.length; i++) {
      await this.#invoke(resource, store, hooks[i].fn, [context]);
    }
  }

  async #runSuiteHooks(suite, name) {
    const context = {
      __proto__: null,
      name: suite.name,
      signal: this.outerSignal,
    };
    await this.#runHooks(suite, name, suite, suite, context);
  }

  #diagnostic(error, loc, level = 'info') {
    this.success = false;
    this.stream.diagnostic({
      __proto__: null,
      message: error?.message ?? `${error}`,
      error,
      level,
      file: loc?.file ?? loc?.[2],
      line: loc?.line ?? loc?.[0],
      column: loc?.column ?? loc?.[1],
    });
  }

  async #completeSubtree(node, error) {
    for (let i = 0; i < node.children.length; i++) {
      const child = node.children[i];
      if (child instanceof Suite) {
        await this.#completeSubtree(child, error);
        child.finished = true;
        child.completion.resolve();
        child.emitDestroy();
      } else {
        await this.#executeBench(child, error);
      }
    }
  }

  async #executeSuite(suite) {
    if (suite.buildError !== null) {
      this.#diagnostic(suite.buildError, suite.loc, 'error');
      await this.#completeSubtree(suite, suite.buildError);
      suite.finished = true;
      suite.completion.resolve();
      suite.emitDestroy();
      return;
    }

    const active = this.#suiteHasActiveBench(suite);
    let beforeError;
    if (active) {
      try {
        await this.#runSuiteHooks(suite, 'before');
      } catch (error) {
        beforeError = error;
        this.#diagnostic(error, suite.loc, 'error');
      }
    }

    if (beforeError !== undefined) {
      await this.#completeSubtree(suite, beforeError);
    } else {
      for (let i = 0; i < suite.children.length; i++) {
        const child = suite.children[i];
        if (child instanceof Suite) {
          await this.#executeSuite(child);
        } else {
          await this.#executeBench(child);
        }
      }
    }

    if (active) {
      try {
        await this.#runSuiteHooks(suite, 'after');
      } catch (error) {
        this.#diagnostic(error, suite.loc, 'error');
      }
    }
    suite.finished = true;
    suite.completion.resolve();
    if (!suite.isRoot) suite.emitDestroy();
  }

  #getHookSuites(benchmark) {
    const suites = [];
    for (let current = benchmark.parent; current !== null; current = current.parent) {
      ArrayPrototypePush(suites, current);
    }
    ArrayPrototypeReverse(suites);
    return suites;
  }

  async #runBenchHooks(benchmark, name, context) {
    const suites = this.#getHookSuites(benchmark);
    if (name === 'afterEach') ArrayPrototypeReverse(suites);
    for (let i = 0; i < suites.length; i++) {
      await this.#runHooks(
        suites[i], name, benchmark, benchmark, context);
    }
  }

  async #runWithStop(benchmark, controller, callback) {
    const signals = [];
    if (this.outerSignal !== undefined) {
      ArrayPrototypePush(signals, this.outerSignal);
    }
    if (benchmark.outerSignal !== undefined &&
        benchmark.outerSignal !== this.outerSignal) {
      ArrayPrototypePush(signals, benchmark.outerSignal);
    }

    for (let i = 0; i < signals.length; i++) {
      if (signals[i].aborted) {
        const error = createAbortError(signals[i]);
        controller.abort(error);
        throw error;
      }
    }

    const stop = PromiseWithResolvers();
    const listeners = [];
    let timer;
    for (let i = 0; i < signals.length; i++) {
      const signal = signals[i];
      ArrayPrototypePush(listeners, addAbortListener(signal, () => {
        const error = createAbortError(signal);
        controller.abort(error);
        stop.reject(error);
      }));
    }
    if (benchmark.timeout !== Infinity) {
      timer = setTimeout(() => {
        const error = new ERR_OPERATION_FAILED(
          `Benchmark timed out after ${benchmark.timeout}ms`);
        controller.abort(error);
        stop.reject(error);
      }, benchmark.timeout);
    }

    const work = callback();
    try {
      if (signals.length === 0 && timer === undefined) return await work;
      return await SafePromiseRace([work, stop.promise]);
    } finally {
      if (timer !== undefined) clearTimeout(timer);
      for (let i = 0; i < listeners.length; i++) {
        listeners[i][SymbolDispose]();
      }
    }
  }

  async #runSample(benchmark, signal) {
    const context = new BenchContext(benchmark, signal);
    try {
      await this.#invoke(
        benchmark, benchmark, benchmark.fn, [context]);
      return context.finish();
    } catch (error) {
      context.close();
      throw error;
    }
  }

  #createResult(benchmark, samples, extra = kEmptyObject) {
    return {
      __proto__: null,
      benchId: benchmark.benchId,
      parentId: benchmark.parentId,
      name: benchmark.name,
      file: benchmark.loc.file,
      line: benchmark.loc.line,
      column: benchmark.loc.column,
      tags: ArrayPrototypeSlice(benchmark.tags),
      params: benchmark.params,
      samples,
      ...extra,
    };
  }

  #recordResult(benchmark, result) {
    benchmark.finished = true;
    benchmark.result = result;
    this.stream.complete(result);
    benchmark.completion.resolve(result);
    benchmark.emitDestroy();
  }

  async #executeBench(benchmark, forcedError = undefined) {
    const duplicateError = this.#duplicateErrors.get(benchmark);
    if (duplicateError !== undefined) {
      this.success = false;
      this.counts.failed++;
      this.#recordResult(benchmark, this.#createResult(
        benchmark,
        [],
        { __proto__: null, error: duplicateError },
      ));
      return;
    }

    const skip = this.#getSkip(benchmark);
    if (skip !== null) {
      this.counts.skipped++;
      this.#recordResult(benchmark, this.#createResult(
        benchmark,
        [],
        { __proto__: null, skip },
      ));
      return;
    }

    if (forcedError !== undefined) {
      this.success = false;
      this.counts.failed++;
      this.#recordResult(benchmark, this.#createResult(
        benchmark,
        [],
        { __proto__: null, error: forcedError },
      ));
      return;
    }

    this.stream.start({
      __proto__: null,
      benchId: benchmark.benchId,
      parentId: benchmark.parentId,
      name: benchmark.name,
      file: benchmark.loc.file,
      line: benchmark.loc.line,
      column: benchmark.loc.column,
      tags: ArrayPrototypeSlice(benchmark.tags),
      params: benchmark.params,
    });

    const controller = new AbortController();
    const samples = [];
    const hookContext = {
      __proto__: null,
      name: benchmark.name,
      params: benchmark.params,
      signal: controller.signal,
    };
    let error;

    try {
      await this.#runWithStop(benchmark, controller, async () => {
        try {
          await this.#runBenchHooks(
            benchmark, 'beforeEach', hookContext);
          const total = benchmark.warmup + benchmark.samples;
          for (let i = 0; i < total; i++) {
            if (controller.signal.aborted) {
              throw controller.signal.reason;
            }
            const sample = await this.#runSample(
              benchmark, controller.signal);
            if (controller.signal.aborted) {
              throw controller.signal.reason;
            }
            if (i >= benchmark.warmup) {
              ArrayPrototypePush(samples, sample);
              this.stream.sample({
                __proto__: null,
                benchId: benchmark.benchId,
                parentId: benchmark.parentId,
                name: benchmark.name,
                index: i - benchmark.warmup,
                ...sample,
              });
            }
            if (i + 1 < total) await eventLoopTurn();
          }
        } finally {
          await this.#runBenchHooks(
            benchmark, 'afterEach', hookContext);
        }
      });
    } catch (cause) {
      error = cause;
    } finally {
      controller.abort();
    }

    if (error !== undefined) {
      this.success = false;
      this.counts.failed++;
      this.#recordResult(benchmark, this.#createResult(
        benchmark,
        samples,
        { __proto__: null, error },
      ));
      return;
    }

    this.counts.completed++;
    this.#recordResult(benchmark, this.#createResult(
      benchmark,
      samples,
      { __proto__: null, summary: summarizeSamples(samples) },
    ));
  }

  #finish(startTime) {
    if (this.state === 'finished') return;
    this.state = 'finished';
    const duration = startTime === undefined ? 0n : hrtime() - startTime;
    this.stream.summary({
      __proto__: null,
      success: this.success,
      counts: this.counts,
      duration_ns: duration,
      file: this.entryFile,
    });
    this.stream.end();
    this.root.finished = true;
    this.root.completion.resolve();
    this.root.emitDestroy();
    this.#storage.disable();
    if (!this.#explicitRun && !this.success) {
      process.exitCode = kGenericUserError;
    }
  }

  async #execute() {
    this.state = 'building';
    const startTime = hrtime();
    await this.#waitForBuild();
    this.#prepare();
    this.state = 'running';
    await this.#executeSuite(this.root);
    this.#finish(startTime);
  }
}

let globalHarness;

function lazyHarness() {
  globalHarness ??= new Harness();
  return globalHarness;
}

function runInParentContext(type) {
  const declare = (name, options, fn, overrides = kEmptyObject) => {
    const harness = lazyHarness();
    const loc = getCallerLocation();
    const declarationOptions = { __proto__: null, ...overrides, loc };
    return type === 'benchmark' ?
      harness.createBench(name, options, fn, declarationOptions) :
      harness.createSuite(name, options, fn, declarationOptions);
  };

  if (type === 'benchmark') {
    declare.skip = (name, options, fn) => declare(
      name, options, fn, { __proto__: null, skip: true });
    declare.only = (name, options, fn) => declare(
      name, options, fn, { __proto__: null, only: true });
  }
  return declare;
}

function hook(name) {
  return (fn, options) => lazyHarness().createHook(name, fn, options);
}

const bench = runInParentContext('benchmark');
const suite = runInParentContext('suite');

function runBenchmarks(options) {
  return lazyHarness().run(options);
}

module.exports = {
  Harness,
  after: hook(kHookNames[0]),
  afterEach: hook(kHookNames[1]),
  before: hook(kHookNames[2]),
  beforeEach: hook(kHookNames[3]),
  bench,
  runBenchmarks,
  suite,
};
