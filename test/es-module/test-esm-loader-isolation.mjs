// Flags: --experimental-loader ./test/fixtures/es-module-loaders/isolation-hook.mjs
import { mustCall } from '../common/index.mjs';
import assert from 'assert';
import { randomBytes } from 'crypto';
import { fileURLToPath } from 'url';
import { Worker, isMainThread, parentPort } from 'worker_threads';

import {
  parentPort as hookParentPort,
  workerData as hookWorkerData,
} from 'test!worker_threads';
import { globalValue } from 'test!globalValue';
import {
  identity as hookIdentity,
  threadId as hookThreadId,
} from 'test!identity';

assert.strictEqual(hookParentPort, null);
assert.strictEqual(hookWorkerData, null);

assert.strictEqual(globalValue, 42);
assert.notStrictEqual(globalThis.globalValue, globalValue);

const hookIdentityData = {
  static: { hookIdentity, hookThreadId },
  unique: randomBytes(16).toString('hex'),
};

if (isMainThread) {
  const worker = new Worker(fileURLToPath(import.meta.url));
  worker.once('message', mustCall((workerMessage) => {
    assert.deepStrictEqual(workerMessage.static, hookIdentityData.static);
    assert.notDeepStrictEqual(workerMessage.unique, hookIdentityData.unique);
  }));
} else {
  parentPort.postMessage(hookIdentityData);
}
