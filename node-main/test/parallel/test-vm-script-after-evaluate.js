'use strict';

// https://github.com/nodejs/node/issues/59541
//
// Promises created in a context using microtaskMode: "aferEvaluate" (meaning
// it has its own microtask queue), when resolved in the surrounding context,
// will schedule a task back onto the inner context queue. This test checks that
// the async execution progresses normally.

const common = require('../common');
const vm = require('vm');

const microtaskMode = 'afterEvaluate';

(async () => {
  const mustNotCall1 = common.mustNotCall();

  await vm.runInNewContext(
    `Promise.resolve()`,
    {}, { microtaskMode });

  // Expected behavior: resolving an promise created in the inner context, from
  // the outer context results in the execution flow falling through, unless the
  // inner context microtask queue is manually drained, which we don't do here.
  mustNotCall1();
})().then(common.mustNotCall());

(async () => {
  const mustCall1 = common.mustCall();
  const mustCall2 = common.mustCall();
  const mustCall3 = common.mustCall();

  // Create a new context.
  const context = vm.createContext({}, { microtaskMode });

  setImmediate(() => {
    // This will drain the context microtask queue, after the `await` statement
    // below, and allow the promise from the inner context, created below, to be
    // resolved in the outer context.
    vm.runInContext('', context);
    mustCall2();
  });

  const inner_promise = vm.runInContext(
    `Promise.resolve()`,
    context);
  mustCall1();

  await inner_promise;
  mustCall3();
})().then(common.mustCall());

{
  const mustNotCall1 = common.mustNotCall();
  const mustCall1 = common.mustCall();

  const context = vm.createContext({ setImmediate, mustNotCall1 }, { microtaskMode });

  // setImmediate() will be run after runInContext() returns, and since the
  // anonymous function passed to `then` is defined in the inner context, the
  // thenable job task will be enqueued on the inner context microtask queue,
  // but at this point, it will not be drained automatically.
  vm.runInContext(`new Promise(setImmediate).then(() => mustNotCall1())`, context);

  mustCall1();
}
