'use strict';
const common = require('../common');

// This test ensures that running vm with breakOnSignt option in multiple
// worker_threads does not crash.
// Issue: https://github.com/nodejs/node/issues/43699
const { Worker } = require('worker_threads');
const vm = require('vm');

// Don't use isMainThread to allow running this test inside a worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  for (let i = 0; i < 10; i++) {
    const worker = new Worker(__filename);
    worker.on('exit', common.mustCall());
  }
} else {
  const ctx = vm.createContext({});
  for (let i = 0; i < 100; i++) {
    vm.runInContext('console.log(1)', ctx, { breakOnSigint: true });
  }
}
