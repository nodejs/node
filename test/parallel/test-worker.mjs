import { mustCall } from '../common/index.mjs';
import assert from 'assert';
import { Worker, isMainThread, parentPort } from 'worker_threads';

const TEST_STRING = 'Hello, world!';

if (isMainThread) {
  const w = new Worker(new URL(import.meta.url));
  w.on('message', mustCall((message) => {
    assert.strictEqual(message, TEST_STRING);
  }));
} else {
  setImmediate(() => {
    process.nextTick(() => {
      parentPort.postMessage(TEST_STRING);
    });
  });
}
