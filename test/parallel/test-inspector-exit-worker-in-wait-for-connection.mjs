// Flags: --inspect=0
import * as common from '../common/index.mjs';
common.skipIfInspectorDisabled();

import { parentPort, isMainThread, Worker } from 'node:worker_threads';
import { fileURLToPath } from 'node:url';
import inspector from 'node:inspector';
import assert from 'node:assert';

// Ref: https://github.com/nodejs/node/issues/52467

if (isMainThread) {
  // worker.terminate() should terminate the worker and the pending
  // inspector.waitForDebugger().
  {
    const worker = new Worker(fileURLToPath(import.meta.url));
    await new Promise((r) => worker.on('message', r));
    worker.on('exit', common.mustCall());
    await worker.terminate();
  }
  // process.exit() should kill the process.
  {
    const worker = new Worker(fileURLToPath(import.meta.url));
    await new Promise((r) => worker.on('message', r));
    process.on('exit', (status) => assert.strictEqual(status, 0));
    setImmediate(() => process.exit());
  }
} else {
  inspector.open();
  parentPort.postMessage('open');
  inspector.waitForDebugger();
}
