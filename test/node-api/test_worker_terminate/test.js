'use strict';
const common = require('../../common');
const assert = require('assert');
const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  // Load the addon in the main thread first.
  // This checks that N-API addons can be loaded from multiple contexts
  // when they are not loaded through NAPI_MODULE().
  require(`./build/${common.buildType}/test_worker_terminate`);

  const counter = new Int32Array(new SharedArrayBuffer(4));
  const worker = new Worker(__filename, { workerData: { counter } });
  worker.on('exit', common.mustCall(() => {
    assert.strictEqual(counter[0], 1);
  }));
  worker.on('error', common.mustNotCall());
} else {
  const { Test } = require(`./build/${common.buildType}/test_worker_terminate`);

  const { counter } = workerData;
  // Test() tries to call a function twice and asserts that the second call does
  // not work because of a pending exception.
  Test(() => {
    Atomics.add(counter, 0, 1);
    process.exit();
  });
}
