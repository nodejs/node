'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

const readable = new Readable({
  read(size) {}
});

const state = readable._readableState;

// Starting off with false initially.
assert.strictEqual(state.reading, false);
assert.strictEqual(state.readingMore, false);

readable.on('data', common.mustCall((data) => {
  // while in a flowing state, should try to read more.
  if (readable.readableFlowing)
    assert.strictEqual(state.readingMore, true);

  // reading as long as we've not ended
  assert.strictEqual(state.reading, !state.ended);
}, 2));

function onStreamEnd() {
  // End of stream; state.reading is false
  // And so should be readingMore.
  assert.strictEqual(state.readingMore, false);
  assert.strictEqual(state.reading, false);
}

readable.on('readable', common.mustCall(() => {
  // 'readable' always gets called before 'end'
  // since 'end' hasn't been emitted, more data could be incoming
  assert.strictEqual(state.readingMore, true);

  // if the stream has ended, we shouldn't be reading
  assert.strictEqual(state.ended, !state.reading);

  const data = readable.read();
  if (data === null) // reached end of stream
    process.nextTick(common.mustCall(onStreamEnd, 1));
}, 2));

readable.on('end', common.mustCall(onStreamEnd));

readable.push('pushed');

// stop emitting 'data' events
readable.pause();

// read() should only be called while operating in paused mode
readable.read(6);

// reading
assert.strictEqual(state.reading, true);
assert.strictEqual(state.readingMore, true);

// resume emitting 'data' events
readable.resume();

// add chunk to front
readable.unshift('unshifted');

// end
readable.push(null);
