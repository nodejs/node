'use strict';
// Flags: --no-warnings --expose-internals
const common = require('../common');
const assert = require('assert');
const test = require('node:test');
const { Duplex, Writable } = require('stream');
const {
  newWritableStreamFromStreamWritable,
  newReadableWritablePairFromDuplex,
} = require('internal/webstreams/adapters');

function isSameError(expected) {
  return common.mustCall((actual) => {
    assert.strictEqual(actual, expected);
    return true;
  });
}

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

test('WritableStream from Node.js stream handles async write error', async () => {
  const error = new Error('boom');
  const writable = new Writable({
    write(_chunk, _encoding, callback) {
      setImmediate(callback, error);
    },
  });
  const writer = Writable.toWeb(writable).getWriter();

  await Promise.all([
    assert.rejects(writer.write(Buffer.from('hello')), isSameError(error)),
    assert.rejects(writer.closed, isSameError(error)),
  ]);
});

test('WritableStream aborts while a native write is pending', async () => {
  const error = new Error('abort');
  let finishWrite;
  let startWrite;
  const writeStarted = new Promise((resolve) => {
    startWrite = resolve;
  });
  const writable = new Writable({
    write(_chunk, _encoding, callback) {
      finishWrite = callback;
      startWrite();
    },
  });
  const writer = Writable.toWeb(writable).getWriter();
  const writePromise = writer.write(Buffer.from('hello'));
  await writeStarted;

  const writeRejected = assert.rejects(writePromise, isSameError(error));
  const closedRejected = assert.rejects(writer.closed, isSameError(error));
  await Promise.all([
    writer.abort(error),
    writeRejected,
    closedRejected,
  ]);

  finishWrite();
  await new Promise(setImmediate);
  assert.strictEqual(writable.destroyed, true);
});

test('WritableStream handles destruction while a write is pending', async () => {
  const error = new Error('destroy');
  let startWrite;
  const writeStarted = new Promise((resolve) => {
    startWrite = resolve;
  });
  const writable = new Writable({
    write(_chunk, _encoding, _callback) {
      startWrite();
    },
  });
  const writer = Writable.toWeb(writable).getWriter();
  const writePromise = writer.write(Buffer.from('hello'));
  await writeStarted;

  const writeRejected = assert.rejects(writePromise, isSameError(error));
  const closedRejected = assert.rejects(writer.closed, isSameError(error));
  writable.destroy(error);
  await Promise.all([writeRejected, closedRejected]);
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
