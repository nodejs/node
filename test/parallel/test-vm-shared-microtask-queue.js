'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

// 1. Creation and type checking
{
  const queue1 = new vm.MicrotaskQueue();
  const queue2 = vm.createMicrotaskQueue();

  assert(vm.isMicrotaskQueue(queue1));
  assert(vm.isMicrotaskQueue(queue2));
  assert(queue1 instanceof vm.MicrotaskQueue);
  assert(queue2 instanceof vm.MicrotaskQueue);

  assert.strictEqual(vm.isMicrotaskQueue({}), false);
  assert.strictEqual(vm.isMicrotaskQueue(null), false);
  assert.strictEqual(vm.isMicrotaskQueue(undefined), false);
  assert.strictEqual(vm.isMicrotaskQueue(42), false);
  assert.strictEqual(vm.isMicrotaskQueue('queue'), false);

  assert.throws(() => {
    vm.MicrotaskQueue();
  }, {
    name: 'TypeError',
    message: "Class constructor MicrotaskQueue cannot be invoked without 'new'",
  });

  assert.throws(() => {
    queue1.runMicrotasks.call({});
  }, {
    code: 'ERR_INVALID_THIS',
    name: 'TypeError',
  });
}

// 2. Options validation
{
  assert.throws(() => {
    vm.createContext({}, { microtaskQueue: {} });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

  assert.throws(() => {
    vm.runInNewContext('1 + 1', {}, { microtaskQueue: 'invalid' });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}

// 3. The exact issue #65555 HTML Window and same-agent iframe checkpoint model
{
  function run(options, drain = () => {}) {
    const trace = [];
    const record = (entry) => trace.push(entry);
    const window = vm.createContext({ record }, options);
    const iframe = vm.createContext({ record }, options);

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

  const expected = ['win-rxn', 'iframe-rxn', 'win-follow-up'];

  // Default contexts share Node's queue, but have not drained before returning
  assert.deepStrictEqual(run(), []);

  // afterEvaluate gives every context a separate queue, draining Window prematurely
  assert.deepStrictEqual(
    run(
      { microtaskMode: 'afterEvaluate' },
      (iframe) => vm.runInContext('', iframe),
    ),
    ['win-rxn', 'win-follow-up', 'iframe-rxn'],
  );

  // Shared microtask queue achieves the HTML specification order
  const queue = vm.createMicrotaskQueue();
  assert.deepStrictEqual(
    run(
      { microtaskQueue: queue },
      () => queue.runMicrotasks(),
    ),
    expected,
  );
}

// 4. Shared microtask queue with microtaskMode: 'afterEvaluate'
{
  const queue = new vm.MicrotaskQueue();
  const trace = [];
  const record = (entry) => trace.push(entry);

  const contextA = vm.createContext({ record }, {
    microtaskQueue: queue,
    microtaskMode: 'afterEvaluate',
  });
  const contextB = vm.createContext({ record }, {
    microtaskQueue: queue,
  });

  vm.runInContext(`
    Promise.resolve().then(() => record('A'));
  `, contextA);

  vm.runInContext(`
    Promise.resolve().then(() => record('B'));
  `, contextB);

  // Context A has afterEvaluate, so evaluation drains the shared queue
  assert.deepStrictEqual(trace, ['A']);

  // Context B does not have afterEvaluate, so microtasks stay until explicitly drained
  assert.deepStrictEqual(trace, ['A']);
  queue.runMicrotasks();
  assert.deepStrictEqual(trace, ['A', 'B']);
}
