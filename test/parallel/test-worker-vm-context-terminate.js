'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const { Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename);
  w.on('online', common.mustCall(() => {
    setTimeout(() => w.terminate(), 50);
  }));
  w.on('error', common.mustNotCall());
  w.on('exit', common.mustCall((code) => assert.strictEqual(code, 1)));
} else {
  while (true)
    vm.runInNewContext('');
}
