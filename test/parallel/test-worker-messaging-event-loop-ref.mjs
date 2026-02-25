import '../common/index.mjs';
import assert from 'node:assert';
import {
  Worker,
  postMessageToThread,
} from 'node:worker_threads';

// postMessageToThread() uses an internal Atomics.waitAsync path, which should keep the event loop alive while awaiting the response.
const workerCode = `
const { parentPort } = require('node:worker_threads');
process.on('workerMessage', () => {});
parentPort.postMessage('ready');
`;

const worker = new Worker(workerCode, { eval: true });
await new Promise((resolve) => worker.once('message', resolve));
worker.unref();

await assert.doesNotReject(postMessageToThread(worker.threadId, { hello: 1 }));
await worker.terminate();
