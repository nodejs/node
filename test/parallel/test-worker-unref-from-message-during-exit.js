'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// This used to crash because the `.unref()` was unexpected while the Worker
// was exiting.

const w = new Worker(`
require('worker_threads').parentPort.postMessage({});
`, { eval: true });
w.on('message', common.mustCall(() => {
  w.unref();
}));

// Wait a bit so that the 'message' event is emitted while the Worker exits.
Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 100);
