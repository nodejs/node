// Test: terminating a worker thread while it evaluates the parent module of a
// top-level-await module.
//
// worker.terminate() used to abort the whole process with a V8 CHECK failure
// (`Check failed: !is_null().`). See https://github.com/nodejs/node/issues/67550
// and the V8 cherry-pick be84cdeee94e.

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { Worker } from 'node:worker_threads';

const worker = new Worker(
  new URL('../fixtures/es-modules/tla/terminate-worker.mjs', import.meta.url),
);

worker.on('error', (err) => assert.fail(err));

// The worker posts 'running' at the top of the parent module body and then
// busy-waits. Terminating at that point used to abort the entire process via a
// V8 CHECK failure in `AsyncModuleExecutionFulfilled`; after the fix it
// terminates cleanly with exit code 1.
worker.on('message', mustCall(async (message) => {
  assert.strictEqual(message, 'running');
  assert.strictEqual(await worker.terminate(), 1);
}));
