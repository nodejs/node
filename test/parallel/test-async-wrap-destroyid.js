'use strict';

const common = require('../common');
const async_wrap = process.binding('async_wrap');
const assert = require('assert');
const async_hooks = require('async_hooks');
const RUNS = 5;
let test_id = null;
let run_cntr = 0;
let hooks = null;

process.on('beforeExit', common.mustCall(() => {
  process.removeAllListeners('uncaughtException');
  hooks.disable();
  assert.strictEqual(test_id, null);
  assert.strictEqual(run_cntr, RUNS);
}));


hooks = async_hooks.createHook({
  destroy(id) {
    if (id === test_id) {
      run_cntr++;
      test_id = null;
    }
  },
}).enable();


(function runner(n) {
  assert.strictEqual(test_id, null);
  if (n <= 0) return;

  test_id = (Math.random() * 1e9) >>> 0;
  async_wrap.addIdToDestroyList(test_id);
  setImmediate(common.mustCall(runner), n - 1);
})(RUNS);
