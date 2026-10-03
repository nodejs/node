'use strict';
require('../common');

// Verifies that Writable.toWeb() sizes queued chunks by their byte length for a
// non-object-mode stream, instead of counting each chunk as one, and that an
// object-mode stream still counts chunks.

const assert = require('node:assert');
const { Writable } = require('node:stream');
const { test } = require('node:test');

function writerFor(objectMode) {
  // The write callback is never invoked, so chunks stay queued.
  const writable = new Writable({ highWaterMark: 100, objectMode, write() {} });
  return Writable.toWeb(writable).getWriter();
}

test('non-object mode sizes chunks by byte length', () => {
  const writer = writerFor(false);

  assert.strictEqual(writer.desiredSize, 100);
  writer.write(new Uint8Array(3));
  assert.strictEqual(writer.desiredSize, 97);
  writer.write(Buffer.alloc(7));
  assert.strictEqual(writer.desiredSize, 90);
});

test('object mode counts chunks', () => {
  const writer = writerFor(true);

  assert.strictEqual(writer.desiredSize, 100);
  writer.write(new Uint8Array(3));
  assert.strictEqual(writer.desiredSize, 99);
});
