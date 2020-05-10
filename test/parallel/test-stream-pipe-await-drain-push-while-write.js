'use strict';
const common = require('../common');
const stream = require('stream');
const assert = require('assert');

const secondChunk = Buffer.alloc(34 * 1024);
const writable = new stream.Writable({
  write: common.mustCall(function(chunk, encoding, cb) {
    if (chunk.length === 32 * 1024 + 33 * 1024) { // first chunk
      assert.strictEqual(
        readable._readableState.awaitDrainWriters,
        null,
      );

      readable.push(secondChunk); // above hwm
      // We should check if awaitDrain counter is increased in the next
      // tick, because awaitDrain is incremented after this method finished

    } else {
      // Second chunk
      assert.strictEqual(chunk, secondChunk);
      assert.strictEqual(readable._readableState.awaitDrainWriters, writable);
    }

    process.nextTick(cb);
  }, 2)
});

// A readable stream which produces two buffers.
const bufs = [Buffer.alloc(32 * 1024), Buffer.alloc(33 * 1024)]; // above hwm
const readable = new stream.Readable({
  read: function() {
    while (bufs.length > 0) {
      this.push(bufs.shift());
    }
  }
});

readable.pipe(writable);
