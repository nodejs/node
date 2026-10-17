// Flags: --experimental-vm-modules
'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const vm = require('vm');

async_hooks.createHook({ init() {} }).enable();

(async () => {
  const context = vm.createContext({}, { microtaskMode: 'afterEvaluate' });
  const module = new vm.SourceTextModule(
    'Promise.resolve().then(() => { while (true); });',
    { context }
  );
  await module.link(common.mustNotCall());

  await assert.rejects(module.evaluate({ timeout: 5 }), {
    code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
  });
  setImmediate(common.mustCall());
})().then(common.mustCall());
