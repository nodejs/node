'use strict';

// eslint-disable-next-line no-unused-vars
const common = require('../common'); // Required by style but unused
const test = require('node:test');
const assert = require('node:assert');
const { DummyWritableStream } = require('stream/web');

test('writes to dummy stream', async () => {
  const stream = new DummyWritableStream();
  const writer = stream.getWriter();
  await writer.write('hello');
  await writer.close();
});

test('DummyWritableStream allows writing and closing without error', async () => {
  const stream = new DummyWritableStream();
  const writer = stream.getWriter();

  // Writing should not throw
  await writer.write('test');

  // // Closing should not throw
  await writer.close();
});

test('DummyWritableStream ignores input and acts as a no-op', async () => {
  const stream = new DummyWritableStream();
  const writer = stream.getWriter();

  await writer.write('foo');
  await writer.write('bar');
  await writer.close();
  writer.releaseLock();

  assert.strictEqual(stream.locked, false);
});
