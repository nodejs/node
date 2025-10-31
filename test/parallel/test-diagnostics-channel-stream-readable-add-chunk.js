'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in Stream diagnostics channels are reporting
// the diagnostics messages for the 'stream.readable.addChunk' channel when
// chunks are added to the readable stream by:
// - readable.push()
// - readable.unshift()

const assert = require('assert');
const dc = require('diagnostics_channel');
const { Readable } = require('stream');

const values = [
  'hello',
  'world',
  '🌍',
];

const objects = [
  { msg: values[0] },
  { msg: values[1] },
  { msg: values[2] },
];

let i = 0;
dc.subscribe('stream.readable.addChunk', common.mustCall(({ stream, chunk }) => {
  if (i < values.length) {
    assert.strictEqual(stream, readable);
    assert.strictEqual(chunk.toString(), values[i]);
  } else {
    assert.strictEqual(stream, objReadable);
    assert.strictEqual(chunk, objects[i - 3]);
  }
  ++i;
}, values.length * 2));

const readable = new Readable({
  read() {}
});

readable.push(values[0]);
readable.push(values[1]);
readable.unshift(values[2]);
readable.push(null);

const objReadable = new Readable({
  objectMode: true,
  read() {}
});
objReadable.push(objects[0]);
objReadable.push(objects[1]);
objReadable.unshift(objects[2]);
objReadable.push(null);
