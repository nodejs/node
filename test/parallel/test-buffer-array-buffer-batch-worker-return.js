// Flags: --expose-internals --expose-gc
'use strict';
const common = require('../common');
const assert = require('node:assert/strict');
const { once } = require('node:events');
const { Worker, isMainThread, parentPort } = require('node:worker_threads');

const size = 1024;
const count = 32;
const rounds = 12;

if (isMainThread) {
  const { internalBinding } = require('internal/test/binding');
  const { createArrayBufferBatch } = internalBinding('buffer');

  (async () => {
    const worker = new Worker(__filename);
    const exit = once(worker, 'exit');
    for (let round = 0; round < rounds; round++) {
      const buffers = createArrayBufferBatch(size, count, 0);
      for (const buffer of buffers) {
        const bytes = new Uint8Array(buffer);
        assert(bytes.every((byte) => byte === 0));
        bytes.fill(round + 1);
      }
      const ack = once(worker, 'message');
      worker.postMessage(buffers, buffers);
      await ack;
    }
    assert.deepStrictEqual(await exit, [0]);
  })().then(common.mustCall());
} else {
  let round = 0;
  const kept = [];
  parentPort.on('message', common.mustCall((buffers) => {
    round++;
    for (const buffer of buffers) {
      const bytes = new Uint8Array(buffer);
      assert(bytes.every((byte) => byte === round));
      bytes.fill(255 - round);
    }
    kept.push(new Uint8Array(buffers.pop()));
    buffers.length = 0;
    setImmediate(common.mustCall(() => {
      global.gc();
      for (const [i, bytes] of kept.entries())
        assert(bytes.every((byte) => byte === 254 - i));
      parentPort.postMessage(round);
      if (round === rounds) parentPort.close();
    }));
  }, rounds));
}
