// Flags: --experimental-vm-modules
'use strict';

// https://github.com/nodejs/node/issues/59541
//
// Promises created in a context using microtaskMode: "aferEvaluate" (meaning it
// has its own microtask queue), when resolved in the surrounding context, will
// schedule a task back onto the inner context queue.

const common = require('../common');
const vm = require('vm');

const microtaskMode = 'afterEvaluate';

(async () => {
  const mustNotCall1 = common.mustNotCall();
  const mustCall1 = common.mustCall();

  const inner = {};

  const context = vm.createContext({ inner }, { microtaskMode });

  const module = new vm.SourceTextModule(
    'inner.promise = Promise.resolve();',
    { context },
  );

  await module.link(mustNotCall1);
  await module.evaluate();

  // Prior to the fix for Issue 59541, the next statement was never executed.
  mustCall1();
})().then(common.mustCall());
