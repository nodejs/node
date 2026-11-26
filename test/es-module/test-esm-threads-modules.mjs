// node:threads/worker and node:threads/messaging can be imported, and their
// named exports are the same objects as those of node:worker_threads.

import '../common/index.mjs';
import assert from 'node:assert';
import * as workerThreads from 'node:worker_threads';
import * as threadsWorker from 'node:threads/worker';
import * as threadsMessaging from 'node:threads/messaging';

for (const namespace of [threadsWorker, threadsMessaging]) {
  for (const name of Object.keys(namespace)) {
    if (name === 'default') continue;
    assert.strictEqual(namespace[name], workerThreads[name], name);
  }
}

await assert.rejects(import('threads/worker'),
                     { code: 'ERR_MODULE_NOT_FOUND' });
