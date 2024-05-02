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

// Test deprecation of .terminate() with callback.
common.expectWarning(
  'DeprecationWarning',
  'Passing a callback to worker.terminate() is deprecated. ' +
  'It returns a Promise instead.', 'DEP0132');

w.on('message', common.mustCall(() => {
  setTimeout(() => {
    w.terminate(common.mustCall()).then(common.mustCall());
  }, 1);
}));
