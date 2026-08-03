import { mustCall } from '../common/index.mjs';
import assert from 'assert';
import { Worker, isMainThread, parentPort } from 'worker_threads';

const kTestString = 'Hello, world!';

if (isMainThread) {
  const w = new Worker(new URL(import.meta.url));
  w.on('message', mustCall((message) => {
    assert.strictEqual(message, kTestString);
  }));
} else {
  setImmediate(() => {
    process.nextTick(() => {
      parentPort.postMessage(kTestString);
    });
  });
}
