'use strict';

const common = require('../common');
const assert = require('assert');
const { isMainThread, parentPort, Worker } = require('worker_threads');
const { Blob } = require('buffer');

if (isMainThread) {
  const blob = new Blob(['hello world']);
  const url = URL.createObjectURL(blob);

  const worker = new Worker(__filename);
  worker.once('message', common.mustCall((value) => {
    assert.deepStrictEqual(value, { size: 11, type: '', text: 'hello world' });
    worker.terminate();
  }));
  worker.once('error', common.mustNotCall());
  worker.once('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));

  worker.postMessage(url);
} else {
  const { resolveObjectURL } = require('buffer');
  parentPort.once('message', async (url) => {
    const blob = resolveObjectURL(url);
    const text = await blob.text();
    parentPort.postMessage({ size: blob.size, type: blob.type, text });
  });
}
