'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Like test-async-hooks-worker-promise.js but doing a trivial counter increase
// after process.exit(). This should not make a difference, but apparently it
// does. This is *also* different from test-async-hooks-worker-promise-3.js,
// in that the statement is an ArrayBuffer access rather than a full method,
// which *also* makes a difference even though it shouldn’t.

const workerData = new Int32Array(new SharedArrayBuffer(4));
const w = new Worker(`
const { createHook } = require('async_hooks');
const { workerData } = require('worker_threads');

setImmediate(async () => {
  createHook({ init() {} }).enable();
  await 0;
  process.exit();
  workerData[0]++;
});
`, { eval: true, workerData });

w.on('exit', common.mustCall(() => assert.strictEqual(workerData[0], 0)));
