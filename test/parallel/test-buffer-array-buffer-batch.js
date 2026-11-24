// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('node:assert/strict');
const { internalBinding } = require('internal/test/binding');
const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const { createArrayBufferBatch } = internalBinding('buffer');

if (isMainThread) {
  const worker = new Worker(__filename);
  let retained;
  worker.once('message', common.mustCall((buffer) => { retained = buffer; }));
  worker.once('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(retained.byteLength, 1024);
    assert.strictEqual(new Uint8Array(retained)[0], 42);
  }));
} else {
  assert.strictEqual(createArrayBufferBatch(1024, 1), undefined);
  assert.strictEqual(createArrayBufferBatch(65536, 2), undefined);
  const buffers = createArrayBufferBatch(1024, 64);
  assert.strictEqual(new Set(buffers).size, 64);
  const first = new Uint8Array(buffers[0]);
  first.fill(42);
  parentPort.postMessage(first.buffer, [first.buffer]);
  assert.strictEqual(first.byteLength, 0);
  for (const buffer of buffers.slice(1)) {
    assert.strictEqual(buffer.byteLength, 1024);
    assert.deepStrictEqual(new Uint8Array(buffer), new Uint8Array(1024));
  }
}
