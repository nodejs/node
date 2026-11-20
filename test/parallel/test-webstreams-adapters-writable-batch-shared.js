'use strict';
require('../common');
const assert = require('node:assert/strict');
const { Writable } = require('node:stream');
const { test } = require('node:test');

test('buffered writes preserve shared backing after repeated fixed-size writes', async () => {
  const warmup = 1200;
  const size = 1024;
  const regular = Buffer.alloc(size);
  const shared = new Uint8Array(new SharedArrayBuffer(size + 8), 8, size);
  const retained = [];
  let seen = 0;
  function accept(chunk) {
    if (seen++ >= warmup) retained.push(chunk);
  }
  const writer = Writable.toWeb(new Writable({
    highWaterMark: 64 * 1024,
    write(chunk, encoding, callback) {
      accept(chunk);
      setImmediate(callback);
    },
    writev(entries, callback) {
      for (const { chunk } of entries) accept(chunk);
      setImmediate(callback);
    },
  })).getWriter();

  for (let i = 0; i < warmup; i++) await writer.write(regular);
  const sources = [shared, regular, shared];
  for (const [index, source] of sources.entries()) {
    source.fill(index + 1);
    await writer.write(source);
    source.fill(255);
  }
  await writer.close();

  assert.strictEqual(seen, warmup + sources.length);
  assert.strictEqual(new Set(retained.map((chunk) => chunk.buffer)).size, sources.length);
  for (const [index, chunk] of retained.entries()) {
    assert(Buffer.isBuffer(chunk));
    assert.strictEqual(chunk.byteOffset, 0);
    assert.strictEqual(chunk.buffer.byteLength, size);
    assert.notStrictEqual(chunk.buffer, sources[index].buffer);
    assert.strictEqual(chunk.buffer instanceof SharedArrayBuffer, sources[index] === shared);
    assert.deepStrictEqual(chunk, Buffer.alloc(size, index + 1));
  }
});
