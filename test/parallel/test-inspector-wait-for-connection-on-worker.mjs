// Flags: --inspect=0
import * as common from '../common/index.mjs';
common.skipIfInspectorDisabled();

import { isMainThread, Worker } from 'node:worker_threads';
import { fileURLToPath } from 'node:url';
import inspector from 'node:inspector';

// Ref: https://github.com/nodejs/node/issues/52467
if (isMainThread) {
  const worker = new Worker(fileURLToPath(import.meta.url));
  await new Promise((r) => setTimeout(r, 1_000));

  worker.on('exit', common.mustCall());

  const timeout = setTimeout(() => {
    process.exit();
  }, 2_000).unref();

  await worker.terminate();

  clearTimeout(timeout);
} else {
  inspector.open();
  inspector.waitForDebugger();
}
