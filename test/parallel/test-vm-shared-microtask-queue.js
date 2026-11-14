// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Tests `vm.createMicrotaskQueue()` and the `microtaskMode: { type: 'manual',
// queue }` form of the `microtaskMode` option, which let multiple contexts
// share where their microtasks are placed and have that queue drained
// explicitly by the embedder, instead of automatically by Node.js.
//
// Refs: https://github.com/nodejs/node/issues/65555

'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// `vm.createMicrotaskQueue()` returns a usable, independent handle each time.
{
  const queue = vm.createMicrotaskQueue();
  assert.strictEqual(typeof queue.runMicrotasks, 'function');
  // Draining an empty queue is a no-op, not an error.
  queue.runMicrotasks();

  const other = vm.createMicrotaskQueue();
  assert.notStrictEqual(queue, other);
}

// The constructor is intentionally not exported from `vm`; the public API for
// creating queues is `vm.createMicrotaskQueue()`.
assert.strictEqual(vm.MicrotaskQueue, undefined);

// `microtaskQueue.runMicrotasks()` throws when called on an unrelated `this`.
{
  const queue = vm.createMicrotaskQueue();
  assert.throws(() => {
    queue.runMicrotasks.call({});
  }, {
    name: 'TypeError',
  });
}

// The exact scenario from the issue: a Window-like context and a same-agent
// iframe-like context need to share a single microtask queue so that
// reactions from both are drained together, in scheduling order.
{
  function run(microtaskMode, drain = () => {}) {
    const trace = [];
    const record = (entry) => trace.push(entry);
    const window = vm.createContext({ record }, { microtaskMode });
    const iframe = vm.createContext({ record }, { microtaskMode });

    vm.runInContext(`
      const pending = new Promise((resolve) => {
        globalThis.resolve = resolve;
      });
      pending.then(() => {
        record('win-rxn');
        Promise.resolve().then(() => record('win-follow-up'));
      });
    `, window);

    vm.runInContext(`
      const pending = new Promise((resolve) => {
        globalThis.resolve = resolve;
      });
      pending.then(() => record('iframe-rxn'));
    `, iframe);

    window.resolveIframe = iframe.resolve;
    vm.runInContext('resolve(); resolveIframe();', window);
    drain(iframe);

    return trace;
  }

  // Default contexts implicitly share Node's own queue, but there is no
  // public API to force a checkpoint before run() returns.
  assert.deepStrictEqual(run(undefined), []);

  // `afterEvaluate` gives every context its own private queue, so `window`'s
  // queue is drained to exhaustion (including its own follow-up) before
  // `iframe`'s reaction ever gets a chance to run.
  assert.deepStrictEqual(
    run('afterEvaluate', (iframe) => vm.runInContext('', iframe)),
    ['win-rxn', 'win-follow-up', 'iframe-rxn'],
  );

  // A single shared queue, drained once, explicitly, by the embedder,
  // reproduces the HTML specification's ordering.
  const queue = vm.createMicrotaskQueue();
  assert.deepStrictEqual(
    run({ type: 'manual', queue }, () => queue.runMicrotasks()),
    ['win-rxn', 'iframe-rxn', 'win-follow-up'],
  );
}

// A shared queue is never drained automatically: microtasks stay queued
// across separate runInContext() calls until runMicrotasks() is called.
{
  const queue = vm.createMicrotaskQueue();
  const trace = [];
  const record = (entry) => trace.push(entry);
  const contextA = vm.createContext({ record }, { microtaskMode: { type: 'manual', queue } });
  const contextB = vm.createContext({ record }, { microtaskMode: { type: 'manual', queue } });

  vm.runInContext("Promise.resolve().then(() => record('a'));", contextA);
  vm.runInContext("Promise.resolve().then(() => record('b'));", contextB);
  assert.deepStrictEqual(trace, []);

  queue.runMicrotasks();
  assert.deepStrictEqual(trace, ['a', 'b']);
}

// All context-creation APIs parse the object form once. In particular, they
// must not read either property a second time after validation.
{
  const script = new vm.Script(
    "Promise.resolve().then(() => record('script.runInNewContext'));",
  );
  const createAndRun = [
    {
      expected: 'createContext',
      run(sandbox, options) {
        const context = vm.createContext(sandbox, options);
        vm.runInContext(
          "Promise.resolve().then(() => record('createContext'));",
          context,
        );
      },
    },
    {
      expected: 'runInNewContext',
      run(sandbox, options) {
        vm.runInNewContext(
          "Promise.resolve().then(() => record('runInNewContext'));",
          sandbox,
          options,
        );
      },
    },
    {
      expected: 'script.runInNewContext',
      run(sandbox, options) {
        script.runInNewContext(sandbox, options);
      },
    },
  ];

  for (const { expected, run } of createAndRun) {
    const queue = vm.createMicrotaskQueue();
    const trace = [];
    const record = (entry) => trace.push(entry);
    let typeGetCount = 0;
    let queueGetCount = 0;
    const microtaskMode = {
      get type() {
        if (++typeGetCount > 1) {
          throw new Error('microtaskMode.type was read more than once');
        }
        return 'manual';
      },
      get queue() {
        if (++queueGetCount > 1) {
          throw new Error('microtaskMode.queue was read more than once');
        }
        return queue;
      },
    };

    run({ record }, { microtaskMode });
    assert.strictEqual(typeGetCount, 1);
    assert.strictEqual(queueGetCount, 1);
    assert.deepStrictEqual(trace, []);
    queue.runMicrotasks();
    assert.deepStrictEqual(trace, [expected]);
  }
}

// `vm.constants.DONT_CONTEXTIFY` sandboxes (no wrapper object, the V8
// context's global proxy is used directly) share a manual queue the same
// way ordinary sandbox objects do.
{
  const queue = vm.createMicrotaskQueue();
  const trace = [];
  const options = { microtaskMode: { type: 'manual', queue } };

  const first = vm.createContext(vm.constants.DONT_CONTEXTIFY, options);
  const second = vm.createContext(vm.constants.DONT_CONTEXTIFY, options);
  first.trace = trace;
  second.trace = trace;

  vm.runInContext("Promise.resolve().then(() => trace.push('first'));", first);
  vm.runInContext("Promise.resolve().then(() => trace.push('second'));", second);
  assert.deepStrictEqual(trace, []);

  queue.runMicrotasks();
  assert.deepStrictEqual(trace, ['first', 'second']);
}

// Invalid `microtaskMode` values.
{
  // Not a recognized string.
  assert.throws(() => {
    vm.createContext({}, { microtaskMode: 'nonsense' });
  }, { code: 'ERR_INVALID_ARG_VALUE' });

  // Neither a string nor an object.
  assert.throws(() => {
    vm.createContext({}, { microtaskMode: 42 });
  }, { code: 'ERR_INVALID_ARG_TYPE' });

  // An object, but with the wrong `type`.
  const queue = vm.createMicrotaskQueue();
  assert.throws(() => {
    vm.createContext({}, { microtaskMode: { type: 'automatic', queue } });
  }, { code: 'ERR_INVALID_ARG_VALUE' });

  // `type: 'manual'`, but `queue` is not a `vm.MicrotaskQueue`.
  assert.throws(() => {
    vm.createContext({}, { microtaskMode: { type: 'manual', queue: {} } });
  }, { code: 'ERR_INVALID_ARG_TYPE' });

  assert.throws(() => {
    vm.createContext({}, { microtaskMode: { type: 'manual' } }); // no queue at all
  }, { code: 'ERR_INVALID_ARG_TYPE' });
}

// `'afterEvaluate'` and `{ type: 'manual', queue }` are mutually exclusive by
// construction: `microtaskMode` is a single value that is either a string or
// an object, so there is no way to request both at once. Passing a plain
// object without `type: 'manual'` set is rejected the same way any other
// malformed object would be, not treated as "also afterEvaluate".
{
  const queue = vm.createMicrotaskQueue();
  assert.throws(() => {
    vm.createContext({}, { microtaskMode: { queue } }); // `type` missing
  }, { code: 'ERR_INVALID_ARG_VALUE' });
}
