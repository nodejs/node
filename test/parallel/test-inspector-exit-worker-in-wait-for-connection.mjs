// Flags: --inspect=0
import * as common from '../common/index.mjs';
common.skipIfInspectorDisabled();

import { isMainThread, Worker } from 'node:worker_threads';
import { fileURLToPath } from 'node:url';
import inspector from 'node:inspector';

// Ref: https://github.com/nodejs/node/issues/52467
// - worker.terminate() should terminate the worker and the pending
//   inspector.waitForDebugger().
if (isMainThread) {
  const worker = new Worker(fileURLToPath(import.meta.url));
  await new Promise((r) => setTimeout(r, 2_000));

  worker.on('exit', common.mustCall());
  await worker.terminate();
} else {
  inspector.open();
  inspector.waitForDebugger();
}
