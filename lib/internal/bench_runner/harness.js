'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeReverse,
  ArrayPrototypeSlice,
  BigInt,
  FunctionPrototypeCall,
  JSONStringify,
  MathCeil,
  Number,
  Promise,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
  RegExp,
  RegExpPrototypeExec,
  SafeMap,
  SafePromiseRace,
  String,
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
  validateBoolean,
  validateFunction,
  validateObject,
  validateUint32,
} = require('internal/validators');
const { queueMicrotask } = require('internal/process/task_queues');
const { getOptionValue } = require('internal/options');
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
const kIsCliRunner = getOptionValue('--bench');
let nextRunId = 0;

function createRunId() {
  return `${process.pid}:${String(hrtime())}:${nextRunId++}`;
}

function eventLoopTurn() {
  return new Promise((resolve) => setImmediate(resolve));
}

function createAbortError(signal) {
  return new AbortError(undefined, { __proto__: null, cause: signal.reason });
}

function createTimeoutError(benchmark) {
  return new ERR_OPERATION_FAILED(
    `Benchmark timed out after ${benchmark.timeout}ms`);
}

class Harness {
  #autoRun;
  #buildPromises = [];
  #duplicateErrors = new SafeMap();
  #explicitRun = false;
  #fileScopeStorage = new AsyncLocalStorage();
  #hasOnly = false;
  #runPromise = null;
  #scheduled = false;
  #starting = false;
  #storage = new AsyncLocalStorage();
  #yieldBetweenSamples;

  constructor(options = kEmptyObject) {
    validateObject(options, 'options');
    const {
      autoRun = true,
      yieldBetweenSamples = true,
    } = options;
    validateBoolean(autoRun, 'options.autoRun');
    validateBoolean(
      yieldBetweenSamples, 'options.yieldBetweenSamples');
    this.#autoRun = autoRun;
    this.#yieldBetweenSamples = yieldBetweenSamples;
    this.runId = createRunId();
    this.fileRunId = this.runId;
    this.entryFile = process.argv?.[1] ?? null;
    this.stream = new BenchmarksStream();
    this.state = 'collecting';
    this.namePattern = null;
    this.outerSignal = undefined;
    this.samples = undefined;
    this.success = true;
    this.warmup = undefined;
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

  getFileScope() {
    return this.#fileScopeStorage.getStore() ?? null;
  }

  runInFileScope(scope, fn) {
    return this.#fileScopeStorage.run(scope, fn);
  }

  setRunScope({ entryFile, fileRunId, runId }) {
    if (this.state !== 'collecting') {
      throw new ERR_INVALID_STATE(
        'benchmark execution scope cannot change after execution has started');
    }
    this.entryFile = entryFile;
    this.fileRunId = fileRunId;
    this.runId = runId;
  }

  #ensureCollecting() {
    if (this.state === 'building' &&
        this.#storage.getStore() instanceof Suite) return;
    if (!this.#explicitRun && !this.#starting &&
        this.state === 'collecting') return;
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
      fileScope: parent.isRoot ? this.getFileScope() : parent.fileScope,
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

    const {
      namePattern,
      samples,
      signal,
      warmup,
      yieldBetweenSamples,
    } = options;
    let nextNamePattern = this.namePattern;
    let nextSamples = this.samples;
    let nextWarmup = this.warmup;
    let nextYieldBetweenSamples = this.#yieldBetweenSamples;
    if (namePattern !== undefined) {
      if (typeof namePattern === 'string') {
        nextNamePattern = new RegExp(namePattern);
      } else if (isRegExp(namePattern)) {
        nextNamePattern = new RegExp(namePattern);
      } else {
        throw new ERR_INVALID_ARG_TYPE(
          'options.namePattern', ['string', 'RegExp'], namePattern);
      }
    }
    if (samples !== undefined) {
      validateUint32(samples, 'options.samples', true);
      nextSamples = samples;
    }
    validateAbortSignal(signal, 'options.signal');
    if (warmup !== undefined) {
      validateUint32(warmup, 'options.warmup');
      nextWarmup = warmup;
    }
    if (yieldBetweenSamples !== undefined) {
      validateBoolean(
        yieldBetweenSamples, 'options.yieldBetweenSamples');
      nextYieldBetweenSamples = yieldBetweenSamples;
    }

    this.namePattern = nextNamePattern;
    this.samples = nextSamples;
    this.outerSignal = signal;
    this.warmup = nextWarmup;
    this.#yieldBetweenSamples = nextYieldBetweenSamples;
  }

  #ensureCanRun() {
    if (this.#explicitRun || this.#starting || this.state !== 'collecting') {
      throw new ERR_INVALID_STATE('benchmark execution has already started');
    }
  }

  run(options = kEmptyObject, force = false) {
    if (kIsCliRunner && !force) {
      throw new ERR_INVALID_STATE(
        'run() cannot be called from a file run with --bench');
    }
    this.#ensureCanRun();
    this.#starting = true;
    try {
      this.configure(options);
      this.#explicitRun = true;
    } finally {
      this.#starting = false;
    }
    this.#schedule(force, true);
    return this.stream;
  }

  #schedule(force = false, explicit = false) {
    if (!force && kIsCliRunner) return;
    if (!explicit && !this.#autoRun) return;
    if (this.#scheduled) return;
    this.#scheduled = true;
    queueMicrotask(() => {
      if (this.#runPromise === null) {
        if (!this.#explicitRun) this.stream.resume();
        this.#runPromise = PromisePrototypeThen(
          this.#execute(), undefined, (error) => this.#recover(error));
      }
    });
  }

  async #recover(error) {
    this.#settleSubtree(this.root, error);
    if (this.state !== 'finished') {
      try {
        await this.#diagnostic(error, undefined, 'error');
      } catch {
        // The stream can fail while reporting the original error.
      }
    }
    try {
      await this.#finish();
    } catch {
      // Stream failure has already been reported to its consumer.
    }
  }

  #settleSubtree(node, error) {
    for (let i = 0; i < node.children.length; i++) {
      const child = node.children[i];
      if (child.finished) continue;
      if (child instanceof Suite) {
        this.#settleSubtree(child, error);
        child.finished = true;
        child.completion.resolve();
      } else {
        this.success = false;
        this.counts.failed++;
        const result = this.#createResult(
          child, [], { __proto__: null, error });
        child.finished = true;
        child.result = result;
        child.completion.resolve(result);
      }
      child.emitDestroy();
    }
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
      const identity = JSONStringify([
        this.#getRecordScope(node).fileRunId,
        node.benchId,
      ]);
      const existing = identities.get(identity);
      if (existing === undefined) {
        identities.set(identity, node);
      } else {
        this.#duplicateErrors.set(node, new ERR_INVALID_STATE(
          `duplicate benchmark identity for "${node.fullName}"`));
      }
    });
  }

  async #emitPlans() {
    const benchmarks = [];
    this.#walk(this.root, (node) => {
      if (node instanceof Bench) ArrayPrototypePush(benchmarks, node);
    });
    for (let i = 0; i < benchmarks.length; i++) {
      const benchmark = benchmarks[i];
      const skip = this.#getSkip(benchmark);
      const data = {
        __proto__: null,
        ...this.#getRecordScope(benchmark),
        benchId: benchmark.benchId,
        parentId: benchmark.parentId,
        name: benchmark.name,
        namePath: ArrayPrototypeSlice(benchmark.namePath),
        file: benchmark.loc.file,
        line: benchmark.loc.line,
        column: benchmark.loc.column,
        tags: ArrayPrototypeSlice(benchmark.tags),
        params: benchmark.params,
        samples: this.samples ?? benchmark.samples,
        warmup: this.warmup ?? benchmark.warmup,
        timeout: benchmark.timeout === Infinity ? null : benchmark.timeout,
        yieldBetweenSamples: this.#yieldBetweenSamples,
        selected: skip === null,
      };
      if (skip !== null) data.skip = skip;
      await this.#waitForStream(this.stream.plan(data));
    }
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
    const hooks = suite.hooks[name];
    for (let i = 0; i < hooks.length; i++) {
      try {
        await this.#invoke(suite, suite, hooks[i].fn, [context]);
      } catch (error) {
        return { __proto__: null, error, hook: hooks[i] };
      }
    }
    return null;
  }

  #getRecordScope(node = undefined) {
    const scope = node?.fileScope;
    if (scope !== null && scope !== undefined) {
      return {
        __proto__: null,
        runId: this.runId,
        fileRunId: scope.fileRunId,
        entryFile: scope.entryFile,
      };
    }
    if (node !== undefined && kIsCliRunner) {
      return {
        __proto__: null,
        runId: this.runId,
        fileRunId: this.fileRunId,
        entryFile: null,
      };
    }
    return {
      __proto__: null,
      runId: this.runId,
      fileRunId: this.fileRunId,
      entryFile: this.entryFile,
    };
  }

  async #waitForStream(canContinue) {
    if (canContinue) return;
    try {
      await this.stream.waitForDrain();
    } catch (error) {
      if (!this.stream.destroyed) throw error;
    }
  }

  async #diagnostic(error, loc, level = 'info', node = undefined) {
    this.success = false;
    await this.#waitForStream(this.stream.diagnostic({
      __proto__: null,
      ...this.#getRecordScope(node),
      message: error?.message ?? `${error}`,
      error,
      level,
      file: loc?.file ?? loc?.[2],
      line: loc?.line ?? loc?.[0],
      column: loc?.column ?? loc?.[1],
    }));
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
      await this.#diagnostic(suite.buildError, suite.loc, 'error', suite);
      await this.#completeSubtree(suite, suite.buildError);
      suite.finished = true;
      suite.completion.resolve();
      suite.emitDestroy();
      return;
    }

    const active = this.#suiteHasActiveBench(suite);
    let beforeError;
    if (active) {
      const failure = await this.#runSuiteHooks(suite, 'before');
      if (failure !== null) {
        beforeError = failure.error;
        await this.#diagnostic(
          failure.error, failure.hook.loc, 'error', failure.hook);
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
      const failure = await this.#runSuiteHooks(suite, 'after');
      if (failure !== null) {
        await this.#diagnostic(
          failure.error, failure.hook.loc, 'error', failure.hook);
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

  async #runWithStop(benchmark, controller, callback, deadline) {
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
    const armTimer = () => {
      let remaining = deadline.value - hrtime();
      if (remaining < 0n) remaining = 0n;
      timer = setTimeout(() => {
        const error = createTimeoutError(benchmark);
        controller.abort(error);
        stop.reject(error);
      }, Number(remaining) / 1e6);
    };
    if (deadline !== null) armTimer();

    const pause = async (work) => {
      if (deadline === null) return work();
      clearTimeout(timer);
      timer = undefined;
      const start = hrtime();
      try {
        return await work();
      } finally {
        deadline.value += hrtime() - start;
        if (!controller.signal.aborted) armTimer();
      }
    };

    const work = callback(pause);
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

  async #runSample(benchmark, signal, phase, index) {
    const context = new BenchContext(
      benchmark, signal, phase, index);
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
      ...this.#getRecordScope(benchmark),
      benchId: benchmark.benchId,
      parentId: benchmark.parentId,
      name: benchmark.name,
      namePath: ArrayPrototypeSlice(benchmark.namePath),
      file: benchmark.loc.file,
      line: benchmark.loc.line,
      column: benchmark.loc.column,
      tags: ArrayPrototypeSlice(benchmark.tags),
      params: benchmark.params,
      samples,
      ...extra,
    };
  }

  async #recordResult(benchmark, result) {
    benchmark.finished = true;
    benchmark.result = result;
    let canContinue;
    try {
      canContinue = this.stream.complete(result);
    } finally {
      benchmark.completion.resolve(result);
      benchmark.emitDestroy();
    }
    await this.#waitForStream(canContinue);
  }

  async #executeBench(benchmark, forcedError = undefined) {
    const duplicateError = this.#duplicateErrors.get(benchmark);
    if (duplicateError !== undefined) {
      this.success = false;
      this.counts.failed++;
      await this.#recordResult(benchmark, this.#createResult(
        benchmark,
        [],
        { __proto__: null, error: duplicateError },
      ));
      return;
    }

    const skip = this.#getSkip(benchmark);
    if (skip !== null) {
      this.counts.skipped++;
      await this.#recordResult(benchmark, this.#createResult(
        benchmark,
        [],
        { __proto__: null, skip },
      ));
      return;
    }

    if (forcedError !== undefined) {
      this.success = false;
      this.counts.failed++;
      await this.#recordResult(benchmark, this.#createResult(
        benchmark,
        [],
        { __proto__: null, error: forcedError },
      ));
      return;
    }

    await this.#waitForStream(this.stream.start({
      __proto__: null,
      ...this.#getRecordScope(benchmark),
      benchId: benchmark.benchId,
      parentId: benchmark.parentId,
      name: benchmark.name,
      namePath: ArrayPrototypeSlice(benchmark.namePath),
      file: benchmark.loc.file,
      line: benchmark.loc.line,
      column: benchmark.loc.column,
      tags: ArrayPrototypeSlice(benchmark.tags),
      params: benchmark.params,
    }));

    const controller = new AbortController();
    const deadline = benchmark.timeout === Infinity ? null : {
      __proto__: null,
      value: hrtime() + BigInt(MathCeil(benchmark.timeout * 1e6)),
    };
    const checkDeadline = () => {
      if (deadline !== null && hrtime() >= deadline.value) {
        const timeoutError = createTimeoutError(benchmark);
        controller.abort(timeoutError);
        throw timeoutError;
      }
    };
    const samples = [];
    const hookContext = {
      __proto__: null,
      name: benchmark.name,
      params: benchmark.params,
      signal: controller.signal,
    };
    let error;

    try {
      await this.#runWithStop(benchmark, controller, async (pause) => {
        try {
          await this.#runBenchHooks(
            benchmark, 'beforeEach', hookContext);
          checkDeadline();
          const warmup = this.warmup ?? benchmark.warmup;
          const total = warmup + (this.samples ?? benchmark.samples);
          for (let i = 0; i < total; i++) {
            if (controller.signal.aborted) {
              throw controller.signal.reason;
            }
            const phase = i < warmup ? 'warmup' : 'measurement';
            const index = phase === 'warmup' ? i : i - warmup;
            const { done, sample } = await this.#runSample(
              benchmark, controller.signal, phase, index);
            checkDeadline();
            if (controller.signal.aborted) {
              throw controller.signal.reason;
            }
            if (i >= warmup) {
              ArrayPrototypePush(samples, sample);
              await pause(() => this.#waitForStream(this.stream.sample({
                __proto__: null,
                ...this.#getRecordScope(benchmark),
                benchId: benchmark.benchId,
                parentId: benchmark.parentId,
                name: benchmark.name,
                namePath: ArrayPrototypeSlice(benchmark.namePath),
                index: i - warmup,
                ...sample,
              })));
            }
            if (done) break;
            if (i + 1 < total && this.#yieldBetweenSamples) {
              await eventLoopTurn();
            }
          }
        } finally {
          await this.#runBenchHooks(
            benchmark, 'afterEach', hookContext);
          checkDeadline();
        }
      }, deadline);
    } catch (cause) {
      error = cause;
    } finally {
      controller.abort();
    }

    if (error !== undefined) {
      this.success = false;
      this.counts.failed++;
      await this.#recordResult(benchmark, this.#createResult(
        benchmark,
        samples,
        { __proto__: null, error },
      ));
      return;
    }

    this.counts.completed++;
    await this.#recordResult(benchmark, this.#createResult(
      benchmark,
      samples,
      { __proto__: null, summary: summarizeSamples(samples) },
    ));
  }

  async #finish(startTime) {
    if (this.state === 'finished') return;
    this.state = 'finished';
    const duration = startTime === undefined ? 0n : hrtime() - startTime;
    try {
      await this.#waitForStream(this.stream.summary({
        __proto__: null,
        ...this.#getRecordScope(),
        success: this.success,
        counts: this.counts,
        duration_ns: duration,
        file: this.entryFile,
      }));
    } catch (error) {
      try {
        await this.#diagnostic(error, undefined, 'error');
      } catch {
        // The stream can fail while reporting the summary listener error.
      }
    } finally {
      this.stream.end();
      this.root.finished = true;
      this.root.completion.resolve();
      this.root.emitDestroy();
      this.#storage.disable();
      if (!this.#explicitRun && !this.success) {
        process.exitCode = kGenericUserError;
      }
    }
  }

  async #execute() {
    this.state = 'building';
    const startTime = hrtime();
    await this.#waitForBuild();
    this.#fileScopeStorage.disable();
    this.#prepare();
    this.state = 'running';
    await this.#emitPlans();
    await this.#executeSuite(this.root);
    await this.#finish(startTime);
  }
}

let globalHarness;

function lazyHarness() {
  globalHarness ??= new Harness();
  return globalHarness;
}

function createDeclaration(type, getHarness) {
  const declare = (name, options, fn, overrides = kEmptyObject) => {
    const harness = getHarness();
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

function createHook(name, getHarness) {
  return (fn, options) => getHarness().createHook(name, fn, options);
}

const bench = createDeclaration('benchmark', lazyHarness);
const suite = createDeclaration('suite', lazyHarness);

function createRunner(options = kEmptyObject) {
  validateObject(options, 'options');
  const harness = new Harness({
    __proto__: null,
    autoRun: false,
    yieldBetweenSamples: options.yieldBetweenSamples === undefined ?
      true : options.yieldBetweenSamples,
  });
  const getHarness = () => harness;
  const runnerBench = createDeclaration('benchmark', getHarness);
  const runnerSuite = createDeclaration('suite', getHarness);
  return {
    __proto__: null,
    after: createHook(kHookNames[0], getHarness),
    afterEach: createHook(kHookNames[1], getHarness),
    before: createHook(kHookNames[2], getHarness),
    beforeEach: createHook(kHookNames[3], getHarness),
    bench: runnerBench,
    describe: runnerSuite,
    run: (runOptions = kEmptyObject) => harness.run(runOptions),
    suite: runnerSuite,
  };
}

function configureRunScope(scope) {
  lazyHarness().setRunScope(scope);
}

function runInFileScope(scope, fn) {
  return lazyHarness().runInFileScope(scope, fn);
}

function runBenchmarks(options, force) {
  return lazyHarness().run(options, force);
}

module.exports = {
  Harness,
  after: createHook(kHookNames[0], lazyHarness),
  afterEach: createHook(kHookNames[1], lazyHarness),
  before: createHook(kHookNames[2], lazyHarness),
  beforeEach: createHook(kHookNames[3], lazyHarness),
  bench,
  configureRunScope,
  createRunId,
  createRunner,
  runInFileScope,
  runBenchmarks,
  suite,
};
