// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

const { internalBinding } = require('internal/test/binding');

const assert = require('assert');
const { Worker } = require('worker_threads');

const processBinding = internalBinding('process_methods');
const originalChdir = processBinding.chdir;

const cwdOriginal = process.cwd();
const i32 = new Int32Array(new SharedArrayBuffer(12));

processBinding.chdir = common.mustCall(function chdir(path) {
  // Signal to the worker that we're inside the chdir call
  Atomics.store(i32, 0, 1);
  Atomics.notify(i32, 0);

  // Pause the chdir call while the worker calls process.cwd(),
  // to simulate a race condition
  Atomics.wait(i32, 1, 0);

  return originalChdir(path);
});

const worker = new Worker(`
  const {
    parentPort,
    workerData: { i32 },
  } = require('worker_threads');

  // Wait until the main thread has entered the chdir call
  Atomics.wait(i32, 0, 0);

  const cwdDuringChdir = process.cwd();

  // Signal the main thread to continue the chdir call
  Atomics.store(i32, 1, 1);
  Atomics.notify(i32, 1);

  // Wait until the main thread has left the chdir call
  Atomics.wait(i32, 2, 0);

  const cwdAfterChdir = process.cwd();
  parentPort.postMessage({ cwdDuringChdir, cwdAfterChdir });
`, {
  eval: true,
  workerData: { i32 },
});

worker.on('exit', common.mustCall());
worker.on('error', common.mustNotCall());
worker.on('message', common.mustCall(({ cwdDuringChdir, cwdAfterChdir }) => {
  assert.strictEqual(cwdDuringChdir, cwdOriginal);
  assert.strictEqual(cwdAfterChdir, process.cwd());
}));

process.chdir('..');

// Signal to the worker that the chdir call is completed
Atomics.store(i32, 2, 1);
Atomics.notify(i32, 2);
