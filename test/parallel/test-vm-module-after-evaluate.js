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
  const mustNotCall2 = common.mustNotCall();

  const inner = {};

  const context = vm.createContext({ inner }, { microtaskMode });

  const module = new vm.SourceTextModule(
    'inner.promise = Promise.resolve();',
    { context },
  );

  await module.link(mustNotCall1);
  await module.evaluate();

  // This is Issue 59541, the next statement is not executed, of course
  // it should be.
  mustNotCall2();
})().then(common.mustNotCall());
