// Flags: --inspect=0
import * as common from '../common/index.mjs';
common.skipIfInspectorDisabled();

import { isMainThread, Worker } from 'node:worker_threads';
import { fileURLToPath } from 'node:url';
import inspector from 'node:inspector';
import assert from 'node:assert';

// Ref: https://github.com/nodejs/node/issues/52467
// - process.exit() should kill the main thread's process.
if (isMainThread) {
  new Worker(fileURLToPath(import.meta.url));
  await new Promise((r) => setTimeout(r, 2_000));

  process.on('exit', (status) => assert.strictEqual(status, 0));
  process.exit();
} else {
  inspector.open();
  inspector.waitForDebugger();
}
