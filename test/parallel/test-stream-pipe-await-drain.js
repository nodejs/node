'use strict';
const common = require('../common');
const stream = require('stream');
const assert = require('assert');

// This is very similar to test-stream-pipe-cleanup-pause.js.

const reader = new stream.Readable();
const writer1 = new stream.Writable();
const writer2 = new stream.Writable();
const writer3 = new stream.Writable();

// 560000 is chosen here because it is larger than the (default) highWaterMark
// and will cause `.write()` to return false
// See: https://github.com/nodejs/node/issues/5820
const buffer = Buffer.allocUnsafe(560000);

reader._read = () => {};

writer1._write = common.mustCall(function(chunk, encoding, cb) {
  this.emit('chunk-received');
  cb();
}, 1);

writer1.once('chunk-received', () => {
  assert.strictEqual(
    reader._readableState.awaitDrainWriters.size,
    0,
    'awaitDrain initial value should be 0, actual is ' +
    reader._readableState.awaitDrainWriters
  );
  setImmediate(() => {
    // This one should *not* get through to writer1 because writer2 is not
    // "done" processing.
    reader.push(buffer);
  });
});

// A "slow" consumer:
writer2._write = common.mustCall((chunk, encoding, cb) => {
  assert.strictEqual(
    reader._readableState.awaitDrainWriters.size,
    1,
    'awaitDrain should be 1 after first push, actual is ' +
    reader._readableState.awaitDrainWriters
  );
  // Not calling cb here to "simulate" slow stream.
  // This should be called exactly once, since the first .write() call
  // will return false.
}, 1);

writer3._write = common.mustCall((chunk, encoding, cb) => {
  assert.strictEqual(
    reader._readableState.awaitDrainWriters.size,
    2,
    'awaitDrain should be 2 after second push, actual is ' +
    reader._readableState.awaitDrainWriters
  );
  // Not calling cb here to "simulate" slow stream.
  // This should be called exactly once, since the first .write() call
  // will return false.
}, 1);

reader.pipe(writer1);
reader.pipe(writer2);
reader.pipe(writer3);
reader.push(buffer);
