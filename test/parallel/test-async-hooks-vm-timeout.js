'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const vm = require('vm');

async_hooks.createHook({ init() {} }).enable();

const executionAsyncId = async_hooks.executionAsyncId();
const context = vm.createContext({}, { microtaskMode: 'afterEvaluate' });

assert.throws(() => {
  vm.runInContext(
    'Promise.resolve().then(() => { while (true); });',
    context,
    { timeout: 5 }
  );
}, {
  code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
});

assert.strictEqual(async_hooks.executionAsyncId(), executionAsyncId);
setImmediate(common.mustCall());
