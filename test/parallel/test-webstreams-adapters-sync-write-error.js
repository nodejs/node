'use strict';
// Flags: --no-warnings --expose-internals
require('../common');
const assert = require('assert');
const test = require('node:test');
const { Duplex, Writable } = require('stream');
const {
  newWritableStreamFromStreamWritable,
  newReadableWritablePairFromDuplex,
} = require('internal/webstreams/adapters');

// Verify that when the underlying Node.js stream throws synchronously from
// write(), the writable web stream properly rejects but does not destroy
// the stream (destroy-on-sync-throw is only used internally by
// CompressionStream/DecompressionStream).

test('WritableStream from Node.js stream handles sync write throw', async () => {
  const error = new TypeError('invalid chunk');
  const writable = new Writable({
    write() {
      throw error;
    },
  });

  const ws = newWritableStreamFromStreamWritable(writable);
  const writer = ws.getWriter();

  await assert.rejects(writer.write('bad'), (err) => {
    assert.strictEqual(err, error);
    return true;
  });

  // Standalone writable should not be destroyed on sync write error
  assert.strictEqual(writable.destroyed, false);
});

test('Duplex-backed pair does NOT destroy on sync write throw', async () => {
  const error = new TypeError('invalid chunk');
  const duplex = new Duplex({
    read() {},
    write() {
      throw error;
    },
  });

  const { writable, readable } = newReadableWritablePairFromDuplex(duplex);
  const writer = writable.getWriter();

  await assert.rejects(writer.write('bad'), (err) => {
    assert.strictEqual(err, error);
    return true;
  });

  // A plain Duplex should NOT be destroyed on sync write error
  assert.strictEqual(duplex.destroyed, false);

  // The readable side should still be usable
  const reader = readable.getReader();
  reader.cancel();
});

test('WritableStream from Node.js stream - valid writes still work', async () => {
  const chunks = [];
  const writable = new Writable({
    write(chunk, _encoding, cb) {
      chunks.push(chunk);
      cb();
    },
  });

  const ws = newWritableStreamFromStreamWritable(writable);
  const writer = ws.getWriter();

  await writer.write(Buffer.from('hello'));
  await writer.write(Buffer.from(' world'));
  await writer.close();

  assert.strictEqual(Buffer.concat(chunks).toString(), 'hello world');
});
