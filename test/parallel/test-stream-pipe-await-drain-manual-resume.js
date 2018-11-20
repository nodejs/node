'use strict';
const common = require('../common');
const stream = require('stream');
const assert = require('assert');

// A consumer stream with a very low highWaterMark, which starts in a state
// where it buffers the chunk it receives rather than indicating that they
// have been consumed.
const writable = new stream.Writable({
  highWaterMark: 5
});

let isCurrentlyBufferingWrites = true;
const queue = [];

writable._write = (chunk, encoding, cb) => {
  if (isCurrentlyBufferingWrites)
    queue.push({ chunk, cb });
  else
    cb();
};

const readable = new stream.Readable({
  read() {}
});

readable.pipe(writable);

readable.once('pause', common.mustCall(() => {
  assert.strictEqual(
    readable._readableState.awaitDrain,
    1,
    'Expected awaitDrain to equal 1 but instead got ' +
    `${readable._readableState.awaitDrain}`
  );
  // First pause, resume manually. The next write() to writable will still
  // return false, because chunks are still being buffered, so it will increase
  // the awaitDrain counter again.

  process.nextTick(common.mustCall(() => {
    readable.resume();
  }));

  readable.once('pause', common.mustCall(() => {
    assert.strictEqual(
      readable._readableState.awaitDrain,
      1,
      '.resume() should not reset the counter but instead got ' +
      `${readable._readableState.awaitDrain}`
    );
    // Second pause, handle all chunks from now on. Once all callbacks that
    // are currently queued up are handled, the awaitDrain drain counter should
    // fall back to 0 and all chunks that are pending on the readable side
    // should be flushed.
    isCurrentlyBufferingWrites = false;
    for (const queued of queue)
      queued.cb();
  }));
}));

readable.push(Buffer.alloc(100));  // Fill the writable HWM, first 'pause'.
readable.push(Buffer.alloc(100));  // Second 'pause'.
readable.push(Buffer.alloc(100));  // Should get through to the writable.
readable.push(null);

writable.on('finish', common.mustCall(() => {
  assert.strictEqual(
    readable._readableState.awaitDrain,
    0,
    'awaitDrain should equal 0 after all chunks are written but instead got' +
    `${readable._readableState.awaitDrain}`
  );
  // Everything okay, all chunks were written.
}));
