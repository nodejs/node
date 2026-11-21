'use strict';
require('../common');
const assert = require('node:assert/strict');
const { Writable } = require('node:stream');
const { test } = require('node:test');

test('the first small byte write can be retained or transferred by the sink', async () => {
  for (const sync of [false, true]) {
    for (const transfer of [false, true]) {
      const input = Buffer.alloc(1026, 37);
      const view = input.subarray(1, 1025);
      let retained;
      const writer = Writable.toWeb(new Writable({
        write(chunk, encoding, callback) {
          retained = transfer ? new Uint8Array(chunk.buffer.transfer()) : chunk;
          if (sync) callback();
          else setImmediate(callback);
        },
      })).getWriter();

      await writer.write(view);
      assert.strictEqual(input.byteLength, 1026);
      view.fill(99);
      await writer.close();
      assert.strictEqual(retained.byteLength, 1024);
      assert(retained.every((byte) => byte === 37));
    }
  }
});

test('a synchronous sink can retain each small write independently', async () => {
  const chunks = [];
  const writer = Writable.toWeb(new Writable({
    write(chunk, encoding, callback) {
      chunks.push(chunk);
      callback();
    },
  })).getWriter();
  const input = Buffer.alloc(1024);
  for (let i = 0; i < 256; i++) {
    input.fill(i);
    await writer.write(input);
  }
  await writer.close();

  assert.strictEqual(chunks.length, 256);
  assert.strictEqual(new Set(chunks.map((chunk) => chunk.buffer)).size, chunks.length);
  const first = new Uint8Array(chunks[0].buffer.transfer());
  assert.strictEqual(input.byteLength, 1024);
  assert(first.every((byte) => byte === 0));
  for (let i = 1; i < chunks.length; i++) {
    assert.strictEqual(chunks[i].byteLength, 1024);
    assert(chunks[i].every((byte) => byte === i));
  }
});
