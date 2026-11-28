// Flags: --expose-gc
'use strict';
const common = require('../common');
const assert = require('node:assert/strict');
const { Writable } = require('node:stream');
const { test } = require('node:test');

test('retained writev chunks survive transfer, GC, and another writer closing', async () => {
  const kept = [];

  function open(size, seed) {
    const input = Buffer.alloc(size);
    let sent = 0;
    let received = 0;
    let batches = 0;

    function consume(chunk) {
      const i = received++;
      const value = (seed + i) % 251;
      assert.strictEqual(chunk.length, size);
      assert.strictEqual(chunk[0], value);
      assert.strictEqual(chunk[size - 1], value);
      if (i > 0 && i % 43 === 0) {
        const saved = i % 2 === 0 ? chunk : Buffer.from(chunk.buffer.transfer());
        kept.push({ saved, value, size });
      }
    }

    const writer = Writable.toWeb(new Writable({
      highWaterMark: 64 * 1024,
      write(chunk, encoding, callback) {
        setImmediate(() => {
          consume(chunk);
          callback();
        });
      },
      writev(entries, callback) {
        batches++;
        setImmediate(() => {
          for (const { chunk } of entries) consume(chunk);
          callback();
        });
      },
    })).getWriter();

    return {
      async send(count) {
        for (let i = 0; i < count; i++) {
          input.fill((seed + sent++) % 251);
          await writer.write(input);
          input.fill(255);
        }
      },
      close: common.mustCall(async () => {
        await writer.close();
        assert.strictEqual(received, sent);
        assert(batches > 0);
      }),
    };
  }

  const first = open(1024, 1);
  const second = open(768, 33);
  await Promise.all([first.send(1024), second.send(1024)]);
  await Promise.all([first.close(), second.send(512)]);
  global.gc();
  const third = open(1536, 77);
  await Promise.all([third.send(512), second.send(512)]);
  await Promise.all([second.close(), third.close()]);

  assert(kept.length > 0);
  for (const { saved, value, size } of kept) {
    assert.strictEqual(saved.buffer.byteLength, size);
    assert.deepStrictEqual(saved, Buffer.alloc(size, value));
  }
});
