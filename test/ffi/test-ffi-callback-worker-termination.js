'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const assert = require('node:assert');
const { test } = require('node:test');
const { spawnAbortingChild } = require('./ffi-callback-test-common');
const { libraryPath } = require('./ffi-test-common');

// Stopping a Worker while it is inside an FFI callback terminates execution.
// That must stop only the Worker instead of aborting the process.
// The Worker must not load test/common: its 'exit' handler would throw from
// inside the callback on process.exit(), which is a real exception.
function runWorker(mode) {
  const workerSource = `
const { parentPort, workerData } = require('node:worker_threads');
const ffi = require('node:ffi');
const { lib, functions } = ffi.dlopen(${JSON.stringify(libraryPath)}, {
  call_int_callback: { arguments: ['pointer', 'i32'], return: 'i32' },
});
const callback = lib.registerCallback(
  { arguments: ['i32'], return: 'i32' },
  () => {
    if (workerData === 'exit') process.exit(0);
    parentPort.postMessage('in callback');
    for (;;);
  },
);
functions.call_int_callback(callback, 21);
`;
  return spawnAbortingChild(`'use strict';
const { Worker } = require('node:worker_threads');
const worker = new Worker(${JSON.stringify(workerSource)}, {
  eval: true,
  workerData: ${JSON.stringify(mode)},
});
worker.on('message', () => {
  if (${JSON.stringify(mode)} === 'shutdown') process.exit(0);
  worker.terminate();
});
worker.on('exit', (code) => console.log('worker exited with code ' + code));`);
}

for (const [mode, stdout] of [
  ['shutdown', ''],
  ['terminate', 'worker exited with code 1\n'],
  ['exit', 'worker exited with code 0\n'],
]) {
  test(`stopping a Worker inside a callback (${mode}) does not abort`, () => {
    const { status, signal, stdout: actual, stderr } = runWorker(mode);
    assert.strictEqual(status, 0, `signal: ${signal}\nstderr: ${stderr}`);
    assert.strictEqual(actual, stdout);
    assert.doesNotMatch(stderr, /Callbacks cannot throw an exception/);
  });
}
