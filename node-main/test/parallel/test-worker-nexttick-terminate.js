'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Checks that terminating in the middle of `process.nextTick()` does not
// Crash the process.

const w = new Worker(`
require('worker_threads').parentPort.postMessage('0');
process.nextTick(() => {
  while(1);
});
`, { eval: true });

w.on('message', common.mustCall(() => {
  setTimeout(() => w.terminate().then(common.mustCall()), 1);
}));
