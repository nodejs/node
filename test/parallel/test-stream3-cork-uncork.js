'use strict';
require('../common');
const assert = require('assert');
const stream = require('stream');
const Writable = stream.Writable;

// Test the buffering behavior of Writable streams.
//
// The call to cork() triggers storing chunks which are flushed
// on calling uncork() in the same tick.
//
// node version target: 0.12

const expectedChunks = ['please', 'buffer', 'me', 'kindly'];
const inputChunks = expectedChunks.slice(0);
let seenChunks = [];
let seenEnd = false;

const w = new Writable();
// Let's arrange to store the chunks.
w._write = function(chunk, encoding, cb) {
  // Default encoding given none was specified.
  assert.strictEqual(encoding, 'buffer');

  seenChunks.push(chunk);
  cb();
};
// Let's record the stream end event.
w.on('finish', () => {
  seenEnd = true;
});

function writeChunks(remainingChunks, callback) {
  const writeChunk = remainingChunks.shift();
  let writeState;

  if (writeChunk) {
    setImmediate(() => {
      writeState = w.write(writeChunk);
      // We were not told to stop writing.
      assert.ok(writeState);

      writeChunks(remainingChunks, callback);
    });
  } else {
    callback();
  }
}

// Do an initial write.
w.write('stuff');
// The write was immediate.
assert.strictEqual(seenChunks.length, 1);
// Reset the chunks seen so far.
seenChunks = [];

// Trigger stream buffering.
w.cork();

// Write the bufferedChunks.
writeChunks(inputChunks, () => {
  // Should not have seen anything yet.
  assert.strictEqual(seenChunks.length, 0);

  // Trigger writing out the buffer.
  w.uncork();

  // Buffered bytes should be seen in current tick.
  assert.strictEqual(seenChunks.length, 4);

  // Did the chunks match.
  for (let i = 0, l = expectedChunks.length; i < l; i++) {
    const seen = seenChunks[i];
    // There was a chunk.
    assert.ok(seen);

    const expected = Buffer.from(expectedChunks[i]);
    // It was what we expected.
    assert.ok(seen.equals(expected));
  }

  setImmediate(() => {
    // The stream should not have been ended.
    assert.ok(!seenEnd);
  });
});
