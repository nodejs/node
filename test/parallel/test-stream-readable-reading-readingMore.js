'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

{
  const readable = new Readable({
    read(size) {}
  });

  const state = readable._readableState;

  // Starting off with false initially.
  assert.strictEqual(state.reading, false);
  assert.strictEqual(state.readingMore, false);

  readable.on('data', common.mustCall((data) => {
    // While in a flowing state with a 'readable' listener
    // we should not be reading more
    if (readable.readableFlowing)
      assert.strictEqual(state.readingMore, true);

    // Reading as long as we've not ended
    assert.strictEqual(state.reading, !state.ended);
  }, 2));

  function onStreamEnd() {
    // End of stream; state.reading is false
    // And so should be readingMore.
    assert.strictEqual(state.readingMore, false);
    assert.strictEqual(state.reading, false);
  }

  const expectedReadingMore = [true, true, false];
  readable.on('readable', common.mustCall(() => {
    // There is only one readingMore scheduled from on('data'),
    // after which everything is governed by the .read() call
    assert.strictEqual(state.readingMore, expectedReadingMore.shift());

    // If the stream has ended, we shouldn't be reading
    assert.strictEqual(state.ended, !state.reading);

    // Consume all the data
    while (readable.read() !== null) {}

    if (expectedReadingMore.length === 0) // Reached end of stream
      process.nextTick(common.mustCall(onStreamEnd, 1));
  }, 3));

  readable.on('end', common.mustCall(onStreamEnd));
  readable.push('pushed');

  readable.read(6);

  // reading
  assert.strictEqual(state.reading, true);
  assert.strictEqual(state.readingMore, true);

  // add chunk to front
  readable.unshift('unshifted');

  // end
  readable.push(null);
}

{
  const readable = new Readable({
    read(size) {}
  });

  const state = readable._readableState;

  // Starting off with false initially.
  assert.strictEqual(state.reading, false);
  assert.strictEqual(state.readingMore, false);

  readable.on('data', common.mustCall((data) => {
    // While in a flowing state without a 'readable' listener
    // we should be reading more
    if (readable.readableFlowing)
      assert.strictEqual(state.readingMore, true);

    // Reading as long as we've not ended
    assert.strictEqual(state.reading, !state.ended);
  }, 2));

  function onStreamEnd() {
    // End of stream; state.reading is false
    // And so should be readingMore.
    assert.strictEqual(state.readingMore, false);
    assert.strictEqual(state.reading, false);
  }

  readable.on('end', common.mustCall(onStreamEnd));
  readable.push('pushed');

  // Stop emitting 'data' events
  assert.strictEqual(state.flowing, true);
  readable.pause();

  // paused
  assert.strictEqual(state.reading, false);
  assert.strictEqual(state.flowing, false);

  readable.resume();
  assert.strictEqual(state.reading, false);
  assert.strictEqual(state.flowing, true);

  // add chunk to front
  readable.unshift('unshifted');

  // end
  readable.push(null);
}

{
  const readable = new Readable({
    read(size) {}
  });

  const state = readable._readableState;

  // Starting off with false initially.
  assert.strictEqual(state.reading, false);
  assert.strictEqual(state.readingMore, false);

  const onReadable = common.mustNotCall;

  readable.on('readable', onReadable);

  readable.on('data', common.mustCall((data) => {
    // Reading as long as we've not ended
    assert.strictEqual(state.reading, !state.ended);
  }, 2));

  readable.removeListener('readable', onReadable);

  function onStreamEnd() {
    // End of stream; state.reading is false
    // And so should be readingMore.
    assert.strictEqual(state.readingMore, false);
    assert.strictEqual(state.reading, false);
  }

  readable.on('end', common.mustCall(onStreamEnd));
  readable.push('pushed');

  // We are still not flowing, we will be resuming in the next tick
  assert.strictEqual(state.flowing, false);

  // Wait for nextTick, so the readableListener flag resets
  process.nextTick(function() {
    readable.resume();

    // Stop emitting 'data' events
    assert.strictEqual(state.flowing, true);
    readable.pause();

    // paused
    assert.strictEqual(state.flowing, false);

    readable.resume();
    assert.strictEqual(state.flowing, true);

    // add chunk to front
    readable.unshift('unshifted');

    // end
    readable.push(null);
  });
}
