'use strict';
// Addons: test_worker_terminate, test_worker_terminate_vtable

const common = require('../../common');
const assert = require('assert');
const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  const { addonPath } = require('../../common/addon-test');

  // Load the addon in the main thread first.
  // This checks that Node-API addons can be loaded from multiple contexts
  // when they are not loaded through NAPI_MODULE().
  require(addonPath);

  const counter = new Int32Array(new SharedArrayBuffer(4));
  const worker = new Worker(__filename, { workerData: { counter, addonPath } });

  worker.on('exit', common.mustCall(() => {
    assert.strictEqual(counter[0], 1);
  }));
  worker.on('error', common.mustNotCall());
} else {
  const { Test } = require(workerData.addonPath);

  const { counter } = workerData;
  // Test() tries to call a function and asserts it fails because of a
  // pending termination exception.
  Test(() => {
    Atomics.add(counter, 0, 1);
    process.exit();
  });
}
