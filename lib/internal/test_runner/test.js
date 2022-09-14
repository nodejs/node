'use strict';
const {
  ArrayPrototypePush,
  ArrayPrototypeReduce,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  ArrayPrototypeUnshift,
  FunctionPrototype,
  MathMax,
  Number,
  ObjectSeal,
  PromisePrototypeThen,
  PromiseResolve,
  ReflectApply,
  SafeMap,
  SafeSet,
  SafePromiseAll,
  SafePromiseRace,
  Symbol,
} = primordials;
const { AsyncResource } = require('async_hooks');
const { once } = require('events');
const { AbortController } = require('internal/abort_controller');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_TEST_FAILURE,
  },
  AbortError,
} = require('internal/errors');
const { getOptionValue } = require('internal/options');
const { TapStream } = require('internal/test_runner/tap_stream');
const { createDeferredCallback, isTestFailureError } = require('internal/test_runner/utils');
const {
  createDeferredPromise,
  kEmptyObject,
} = require('internal/util');
const { isPromise } = require('internal/util/types');
const {
  validateAbortSignal,
  validateNumber,
  validateOneOf,
  validateUint32,
} = require('internal/validators');
const { setTimeout } = require('timers/promises');
const { TIMEOUT_MAX } = require('internal/timers');
const { cpus } = require('os');
const { bigint: hrtime } = process.hrtime;
const kCallbackAndPromisePresent = 'callbackAndPromisePresent';
const kCancelledByParent = 'cancelledByParent';
const kParentAlreadyFinished = 'parentAlreadyFinished';
const kSubtestsFailed = 'subtestsFailed';
const kTestCodeFailure = 'testCodeFailure';
const kTestTimeoutFailure = 'testTimeoutFailure';
const kHookFailure = 'hookFailed';
const kDefaultIndent = '    ';
const kDefaultTimeout = null;
const noop = FunctionPrototype;
const isTestRunner = getOptionValue('--test');
const testOnlyFlag = !isTestRunner && getOptionValue('--test-only');
const kShouldAbort = Symbol('kShouldAbort');
const kRunHook = Symbol('kRunHook');
const kHookNames = ObjectSeal(['before', 'after', 'beforeEach', 'afterEach']);
const kUnwrapErrors = new SafeSet()
  .add(kTestCodeFailure).add(kHookFailure)
  .add('uncaughtException').add('unhandledRejection');


function stopTest(timeout, signal) {
  if (timeout === kDefaultTimeout) {
    return once(signal, 'abort');
  }
  return PromisePrototypeThen(setTimeout(timeout, null, { ref: false, signal }), () => {
    throw new ERR_TEST_FAILURE(
      `test timed out after ${timeout}ms`,
      kTestTimeoutFailure
    );
  });
}

class TestContext {
  #test;

  constructor(test) {
    this.#test = test;
  }

  get signal() {
    return this.#test.signal;
  }

  get name() {
    return this.#test.name;
  }

  diagnostic(message) {
    this.#test.diagnostic(message);
  }

  runOnly(value) {
    this.#test.runOnlySubtests = !!value;
  }

  skip(message) {
    this.#test.skip(message);
  }

  todo(message) {
    this.#test.todo(message);
  }

  test(name, options, fn) {
    // eslint-disable-next-line no-use-before-define
    const subtest = this.#test.createSubtest(Test, name, options, fn);

    return subtest.start();
  }

  beforeEach(fn, options) {
    this.#test.createHook('beforeEach', fn, options);
  }

  afterEach(fn, options) {
    this.#test.createHook('afterEach', fn, options);
  }
}

class Test extends AsyncResource {
  #abortController;
  #outerSignal;

  constructor(options) {
    super('Test');

    let { fn, name, parent, skip } = options;
    const { concurrency, only, timeout, todo, signal } = options;

    if (typeof fn !== 'function') {
      fn = noop;
    }

    if (typeof name !== 'string' || name === '') {
      name = fn.name || '<anonymous>';
    }

    if (!(parent instanceof Test)) {
      parent = null;
    }

    if (parent === null) {
      this.concurrency = 1;
      this.indent = '';
      this.indentString = kDefaultIndent;
      this.only = testOnlyFlag;
      this.reporter = new TapStream();
      this.runOnlySubtests = this.only;
      this.testNumber = 0;
      this.timeout = kDefaultTimeout;
    } else {
      const indent = parent.parent === null ? parent.indent :
        parent.indent + parent.indentString;

      this.concurrency = parent.concurrency;
      this.indent = indent;
      this.indentString = parent.indentString;
      this.only = only ?? !parent.runOnlySubtests;
      this.reporter = parent.reporter;
      this.runOnlySubtests = !this.only;
      this.testNumber = parent.subtests.length + 1;
      this.timeout = parent.timeout;
    }

    switch (typeof concurrency) {
      case 'number':
        validateUint32(concurrency, 'options.concurrency', 1);
        this.concurrency = concurrency;
        break;

      case 'boolean':
        if (concurrency) {
          // TODO(cjihrig): Use uv_available_parallelism() once it lands.
          this.concurrency = parent === null ? MathMax(cpus().length - 1, 1) : Infinity;
        } else {
          this.concurrency = 1;
        }
        break;

      default:
        if (concurrency != null)
          throw new ERR_INVALID_ARG_TYPE('options.concurrency', ['boolean', 'number'], concurrency);
    }

    if (timeout != null && timeout !== Infinity) {
      validateNumber(timeout, 'options.timeout', 0, TIMEOUT_MAX);
      this.timeout = timeout;
    }

    if (testOnlyFlag && !this.only) {
      skip = '\'only\' option not set';
    }

    if (skip) {
      fn = noop;
    }

    this.#abortController = new AbortController();
    this.#outerSignal = signal;
    this.signal = this.#abortController.signal;

    validateAbortSignal(signal, 'options.signal');
    this.#outerSignal?.addEventListener('abort', this.#abortHandler);


    this.fn = fn;
    this.name = name;
    this.parent = parent;
    this.cancelled = false;
    this.skipped = !!skip;
    this.isTodo = !!todo;
    this.startTime = null;
    this.endTime = null;
    this.passed = false;
    this.error = null;
    this.diagnostics = [];
    this.message = typeof skip === 'string' ? skip :
      typeof todo === 'string' ? todo : null;
    this.activeSubtests = 0;
    this.pendingSubtests = [];
    this.readySubtests = new SafeMap();
    this.subtests = [];
    this.hooks = {
      before: [],
      after: [],
      beforeEach: [],
      afterEach: [],
    };
    this.waitingOn = 0;
    this.finished = false;
  }

  hasConcurrency() {
    return this.concurrency > this.activeSubtests;
  }

  addPendingSubtest(deferred) {
    ArrayPrototypePush(this.pendingSubtests, deferred);
  }

  async processPendingSubtests() {
    while (this.pendingSubtests.length > 0 && this.hasConcurrency()) {
      const deferred = ArrayPrototypeShift(this.pendingSubtests);
      await deferred.test.run();
      deferred.resolve();
    }
  }

  addReadySubtest(subtest) {
    this.readySubtests.set(subtest.testNumber, subtest);
  }

  processReadySubtestRange(canSend) {
    const start = this.waitingOn;
    const end = start + this.readySubtests.size;

    for (let i = start; i < end; i++) {
      const subtest = this.readySubtests.get(i);

      // Check if the specified subtest is in the map. If it is not, return
      // early to avoid trying to process any more tests since they would be
      // out of order.
      if (subtest === undefined) {
        return;
      }

      // Call isClearToSend() in the loop so that it is:
      // - Only called if there are results to report in the correct order.
      // - Guaranteed to only be called a maximum of once per call to
      //   processReadySubtestRange().
      canSend = canSend || this.isClearToSend();

      if (!canSend) {
        return;
      }

      if (i === 1 && this.parent !== null) {
        this.reporter.subtest(this.indent, this.name);
      }

      // Report the subtest's results and remove it from the ready map.
      subtest.finalize();
      this.readySubtests.delete(i);
    }
  }

  createSubtest(Factory, name, options, fn, overrides) {
    if (typeof name === 'function') {
      fn = name;
    } else if (name !== null && typeof name === 'object') {
      fn = options;
      options = name;
    } else if (typeof options === 'function') {
      fn = options;
    }

    if (options === null || typeof options !== 'object') {
      options = kEmptyObject;
    }

    let parent = this;

    // If this test has already ended, attach this test to the root test so
    // that the error can be properly reported.
    const preventAddingSubtests = this.finished || this.buildPhaseFinished;
    if (preventAddingSubtests) {
      while (parent.parent !== null) {
        parent = parent.parent;
      }
    }

    const test = new Factory({ __proto__: null, fn, name, parent, ...options, ...overrides });

    if (parent.waitingOn === 0) {
      parent.waitingOn = test.testNumber;
    }

    if (preventAddingSubtests) {
      test.startTime = test.startTime || hrtime();
      test.fail(
        new ERR_TEST_FAILURE(
          'test could not be started because its parent finished',
          kParentAlreadyFinished
        )
      );
    }

    ArrayPrototypePush(parent.subtests, test);
    return test;
  }

  #abortHandler = () => {
    this.cancel(this.#outerSignal?.reason || new AbortError('The test was aborted'));
  };

  cancel(error) {
    if (this.endTime !== null) {
      return;
    }

    this.fail(error ||
      new ERR_TEST_FAILURE(
        'test did not finish before its parent and was cancelled',
        kCancelledByParent
      )
    );
    this.startTime = this.startTime || this.endTime; // If a test was canceled before it was started, e.g inside a hook
    this.cancelled = true;
    this.#abortController.abort();
  }

  createHook(name, fn, options) {
    validateOneOf(name, 'hook name', kHookNames);
    // eslint-disable-next-line no-use-before-define
    const hook = new TestHook(fn, options);
    ArrayPrototypePush(this.hooks[name], hook);
    return hook;
  }

  fail(err) {
    if (this.error !== null) {
      return;
    }

    this.endTime = hrtime();
    this.passed = false;
    this.error = err;
  }

  pass() {
    if (this.endTime !== null) {
      return;
    }

    this.endTime = hrtime();
    this.passed = true;
  }

  skip(message) {
    this.skipped = true;
    this.message = message;
  }

  todo(message) {
    this.isTodo = true;
    this.message = message;
  }

  diagnostic(message) {
    ArrayPrototypePush(this.diagnostics, message);
  }

  start() {
    // If there is enough available concurrency to run the test now, then do
    // it. Otherwise, return a Promise to the caller and mark the test as
    // pending for later execution.
    if (!this.parent.hasConcurrency()) {
      const deferred = createDeferredPromise();

      deferred.test = this;
      this.parent.addPendingSubtest(deferred);
      return deferred.promise;
    }

    return this.run();
  }

  [kShouldAbort]() {
    if (this.signal.aborted) {
      return true;
    }
    if (this.#outerSignal?.aborted) {
      this.cancel(this.#outerSignal.reason || new AbortError('The test was aborted'));
      return true;
    }
  }

  getRunArgs() {
    const ctx = new TestContext(this);
    return { ctx, args: [ctx] };
  }

  async [kRunHook](hook, args) {
    validateOneOf(hook, 'hook name', kHookNames);
    try {
      await ArrayPrototypeReduce(this.hooks[hook], async (prev, hook) => {
        await prev;
        await hook.run(args);
        if (hook.error) {
          throw hook.error;
        }
      }, PromiseResolve());
    } catch (err) {
      const error = new ERR_TEST_FAILURE(`failed running ${hook} hook`, kHookFailure);
      error.cause = isTestFailureError(err) ? err.cause : err;
      throw error;
    }
  }

  async run() {
    if (this.parent !== null) {
      this.parent.activeSubtests++;
    }
    this.startTime = hrtime();

    if (this[kShouldAbort]()) {
      this.postRun();
      return;
    }

    try {
      const { args, ctx } = this.getRunArgs();
      if (this.parent?.hooks.beforeEach.length > 0) {
        await this.parent[kRunHook]('beforeEach', { args, ctx });
      }
      const stopPromise = stopTest(this.timeout, this.signal);
      const runArgs = ArrayPrototypeSlice(args);
      ArrayPrototypeUnshift(runArgs, this.fn, ctx);

      if (this.fn.length === runArgs.length - 1) {
        // This test is using legacy Node.js error first callbacks.
        const { promise, cb } = createDeferredCallback();

        ArrayPrototypePush(runArgs, cb);
        const ret = ReflectApply(this.runInAsyncScope, this, runArgs);

        if (isPromise(ret)) {
          this.fail(new ERR_TEST_FAILURE(
            'passed a callback but also returned a Promise',
            kCallbackAndPromisePresent
          ));
          await SafePromiseRace([ret, stopPromise]);
        } else {
          await SafePromiseRace([PromiseResolve(promise), stopPromise]);
        }
      } else {
        // This test is synchronous or using Promises.
        const promise = ReflectApply(this.runInAsyncScope, this, runArgs);
        await SafePromiseRace([PromiseResolve(promise), stopPromise]);
      }

      if (this[kShouldAbort]()) {
        this.postRun();
        return;
      }

      if (this.parent?.hooks.afterEach.length > 0) {
        await this.parent[kRunHook]('afterEach', { args, ctx });
      }

      this.pass();
    } catch (err) {
      if (isTestFailureError(err)) {
        if (err.failureType === kTestTimeoutFailure) {
          this.cancel(err);
        } else {
          this.fail(err);
        }
      } else {
        this.fail(new ERR_TEST_FAILURE(err, kTestCodeFailure));
      }
    }

    // Clean up the test. Then, try to report the results and execute any
    // tests that were pending due to available concurrency.
    this.postRun();
  }

  postRun(pendingSubtestsError) {
    const counters = { __proto__: null, failed: 0, passed: 0, cancelled: 0, skipped: 0, todo: 0, totalFailed: 0 };

    // If the test was failed before it even started, then the end time will
    // be earlier than the start time. Correct that here.
    if (this.endTime < this.startTime) {
      this.endTime = hrtime();
    }
    this.startTime ??= this.endTime;

    // The test has run, so recursively cancel any outstanding subtests and
    // mark this test as failed if any subtests failed.
    this.pendingSubtests = [];
    for (let i = 0; i < this.subtests.length; i++) {
      const subtest = this.subtests[i];

      if (!subtest.finished) {
        subtest.cancel(pendingSubtestsError);
        subtest.postRun(pendingSubtestsError);
      }

      // Check SKIP and TODO tests first, as those should not be counted as
      // failures.
      if (subtest.skipped) {
        counters.skipped++;
      } else if (subtest.isTodo) {
        counters.todo++;
      } else if (subtest.cancelled) {
        counters.cancelled++;
      } else if (!subtest.passed) {
        counters.failed++;
      } else {
        counters.passed++;
      }

      if (!subtest.passed) {
        counters.totalFailed++;
      }
    }

    if ((this.passed || this.parent === null) && counters.totalFailed > 0) {
      const subtestString = `subtest${counters.totalFailed > 1 ? 's' : ''}`;
      const msg = `${counters.totalFailed} ${subtestString} failed`;

      this.fail(new ERR_TEST_FAILURE(msg, kSubtestsFailed));
    }

    this.#outerSignal?.removeEventListener('abort', this.#abortHandler);

    if (this.parent !== null) {
      this.parent.activeSubtests--;
      this.parent.addReadySubtest(this);
      this.parent.processReadySubtestRange(false);
      this.parent.processPendingSubtests();
    } else if (!this.reported) {
      this.reported = true;
      this.reporter.plan(this.indent, this.subtests.length);

      for (let i = 0; i < this.diagnostics.length; i++) {
        this.reporter.diagnostic(this.indent, this.diagnostics[i]);
      }

      this.reporter.diagnostic(this.indent, `tests ${this.subtests.length}`);
      this.reporter.diagnostic(this.indent, `pass ${counters.passed}`);
      this.reporter.diagnostic(this.indent, `fail ${counters.failed}`);
      this.reporter.diagnostic(this.indent, `cancelled ${counters.cancelled}`);
      this.reporter.diagnostic(this.indent, `skipped ${counters.skipped}`);
      this.reporter.diagnostic(this.indent, `todo ${counters.todo}`);
      this.reporter.diagnostic(this.indent, `duration_ms ${this.#duration()}`);
      this.reporter.push(null);
    }
  }

  isClearToSend() {
    return this.parent === null ||
      (
        this.parent.waitingOn === this.testNumber && this.parent.isClearToSend()
      );
  }

  finalize() {
    // By the time this function is called, the following can be relied on:
    // - The current test has completed or been cancelled.
    // - All of this test's subtests have completed or been cancelled.
    // - It is the current test's turn to report its results.

    // Report any subtests that have not been reported yet. Since all of the
    // subtests have finished, it's safe to pass true to
    // processReadySubtestRange(), which will finalize all remaining subtests.
    this.processReadySubtestRange(true);

    // Output this test's results and update the parent's waiting counter.
    if (this.subtests.length > 0) {
      this.reporter.plan(this.subtests[0].indent, this.subtests.length);
    } else {
      this.reporter.subtest(this.indent, this.name);
    }

    this.report();
    this.parent.waitingOn++;
    this.finished = true;
  }

  #duration() {
    // Duration is recorded in BigInt nanoseconds. Convert to milliseconds.
    return Number(this.endTime - this.startTime) / 1_000_000;
  }

  report() {
    let directive;

    if (this.skipped) {
      directive = this.reporter.getSkip(this.message);
    } else if (this.isTodo) {
      directive = this.reporter.getTodo(this.message);
    }

    if (this.passed) {
      this.reporter.ok(this.indent, this.testNumber, this.name, this.#duration(), directive);
    } else {
      this.reporter.fail(this.indent, this.testNumber, this.name, this.#duration(), this.error, directive);
    }

    for (let i = 0; i < this.diagnostics.length; i++) {
      this.reporter.diagnostic(this.indent, this.diagnostics[i]);
    }
  }
}

class TestHook extends Test {
  #args;
  constructor(fn, options) {
    if (options === null || typeof options !== 'object') {
      options = kEmptyObject;
    }
    const { timeout, signal } = options;
    super({ __proto__: null, fn, timeout, signal });
  }
  run(args) {
    this.#args = args;
    return super.run();
  }
  getRunArgs() {
    return this.#args;
  }
  postRun() {
  }
}

class ItTest extends Test {
  constructor(opt) { super(opt); } // eslint-disable-line no-useless-constructor
  getRunArgs() {
    return { ctx: { signal: this.signal, name: this.name }, args: [] };
  }
}
class Suite extends Test {
  constructor(options) {
    super(options);

    try {
      const { ctx, args } = this.getRunArgs();
      this.buildSuite = PromisePrototypeThen(
        PromiseResolve(this.runInAsyncScope(this.fn, ctx, args)),
        undefined,
        (err) => {
          this.fail(new ERR_TEST_FAILURE(err, kTestCodeFailure));
        });
    } catch (err) {
      this.fail(new ERR_TEST_FAILURE(err, kTestCodeFailure));
    }
    this.fn = () => {};
    this.buildPhaseFinished = true;
  }

  getRunArgs() {
    return { ctx: { signal: this.signal, name: this.name }, args: [] };
  }

  async run() {
    try {
      this.parent.activeSubtests++;
      await this.buildSuite;
      this.startTime = hrtime();

      if (this[kShouldAbort]()) {
        this.subtests = [];
        this.postRun();
        return;
      }


      const hookArgs = this.getRunArgs();
      await this[kRunHook]('before', hookArgs);
      const stopPromise = stopTest(this.timeout, this.signal);
      const subtests = this.skipped || this.error ? [] : this.subtests;
      const promise = SafePromiseAll(subtests, (subtests) => subtests.start());

      await SafePromiseRace([promise, stopPromise]);
      await this[kRunHook]('after', hookArgs);
      this.pass();
    } catch (err) {
      if (isTestFailureError(err)) {
        this.fail(err);
      } else {
        this.fail(new ERR_TEST_FAILURE(err, kTestCodeFailure));
      }
    }

    this.postRun();
  }
}

module.exports = {
  ItTest,
  kCancelledByParent,
  kDefaultIndent,
  kSubtestsFailed,
  kTestCodeFailure,
  kUnwrapErrors,
  Suite,
  Test,
};
