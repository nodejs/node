'use strict';
const common = require('../common');
const assert = require('node:assert/strict');
const { Writable } = require('node:stream');
const { test } = require('node:test');

test('two writers can reuse a source while their native writes are buffered', async () => {
  const backing = new ArrayBuffer(1040);
  const source = new Uint8Array(backing);
  const sizeOf = (i) => (i % 2 === 0 ? 512 : 1024);
  const writes = 320;

  function makeWriter(offset) {
    let next = 0;
    let batches = 0;
    function check(chunk) {
      const i = next++;
      const size = sizeOf(i);
      assert(Buffer.isBuffer(chunk));
      assert.deepStrictEqual(chunk, Buffer.alloc(size, (i + offset) % 251));
      if (i > 0) {
        assert.strictEqual(chunk.buffer.byteLength, size);
        chunk.buffer.transfer();
      }
    }
    const sink = new Writable({
      highWaterMark: 64 * 1024,
      write(chunk, encoding, callback) {
        setImmediate(() => {
          check(chunk);
          callback();
        });
      },
      writev(entries, callback) {
        batches++;
        setImmediate(() => {
          for (const { chunk } of entries) check(chunk);
          callback();
        });
      },
    });
    const writer = Writable.toWeb(sink).getWriter();
    return {
      writer,
      verify: common.mustCall(() => {
        assert.strictEqual(next, writes);
        assert(batches > 0);
      }),
    };
  }

  const left = makeWriter(0);
  const right = makeWriter(7);
  for (let i = 0; i < writes; i++) {
    const size = sizeOf(i);
    source.fill(i % 251);
    await left.writer.write(new Uint8Array(backing, 8, size));
    source.fill((i + 7) % 251);
    await right.writer.write(i % 2 === 0 ?
      new DataView(backing, 8, size) : new Uint16Array(backing, 8, size / 2));
    source.fill(255);
  }
  await Promise.all([left.writer.close(), right.writer.close()]);
  left.verify();
  right.verify();
});
