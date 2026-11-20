'use strict';
require('../common');
const assert = require('node:assert/strict');
const { Writable } = require('node:stream');
const { test } = require('node:test');

test('transferring one buffered write leaves the other writes intact', async () => {
  const source = Buffer.alloc(1032);
  let next = 0;
  let batches = 0;
  function sizeOf(i) {
    return i >= 320 && i % 8 === 7 ? 512 : 1024;
  }
  function check(chunk) {
    const value = next++;
    assert.deepStrictEqual(chunk, Buffer.alloc(sizeOf(value), value));
  }

  const stream = new Writable({
    highWaterMark: 64 * 1024,
    write(chunk, encoding, callback) {
      setImmediate(() => {
        check(chunk);
        callback();
      });
    },
    writev(chunks, callback) {
      batches++;
      setImmediate(() => {
        for (const { chunk } of chunks) {
          check(chunk);
          chunk.buffer.transfer();
        }
        callback();
      });
    },
  });
  const writer = Writable.toWeb(stream).getWriter();
  for (let i = 0; i < 384; i++) {
    const view = source.subarray(8, 8 + sizeOf(i));
    view.fill(i);
    await writer.write(view);
    view.fill(255);
  }
  await writer.close();
  assert(batches > 0);
  assert.strictEqual(next, 384);
  assert.strictEqual(source.byteLength, 1032);
});
