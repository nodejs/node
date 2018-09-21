'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;

const readable = new Readable({
  read: () => {}
});

// Initialized to false.
assert.strictEqual(readable._readableState.needReadable, false);

readable.on('readable', common.mustCall(() => {
  // When the readable event fires, needReadable is reset.
  assert.strictEqual(readable._readableState.needReadable, false);
  readable.read();
}));

// If a readable listener is attached, then a readable event is needed.
assert.strictEqual(readable._readableState.needReadable, true);

readable.push('foo');
readable.push(null);

readable.on('end', common.mustCall(() => {
  // No need to emit readable anymore when the stream ends.
  assert.strictEqual(readable._readableState.needReadable, false);
}));

const asyncReadable = new Readable({
  read: () => {}
});

asyncReadable.on('readable', common.mustCall(() => {
  if (asyncReadable.read() !== null) {
    // After each read(), the buffer is empty.
    // If the stream doesn't end now,
    // then we need to notify the reader on future changes.
    assert.strictEqual(asyncReadable._readableState.needReadable, true);
  }
}, 2));

process.nextTick(common.mustCall(() => {
  asyncReadable.push('foooo');
}));
process.nextTick(common.mustCall(() => {
  asyncReadable.push('bar');
}));
setImmediate(common.mustCall(() => {
  asyncReadable.push(null);
  assert.strictEqual(asyncReadable._readableState.needReadable, false);
}));

const flowing = new Readable({
  read: () => {}
});

// Notice this must be above the on('data') call.
flowing.push('foooo');
flowing.push('bar');
flowing.push('quo');
process.nextTick(common.mustCall(() => {
  flowing.push(null);
}));

// When the buffer already has enough data, and the stream is
// in flowing mode, there is no need for the readable event.
flowing.on('data', common.mustCall(function(data) {
  assert.strictEqual(flowing._readableState.needReadable, false);
}, 3));

const slowProducer = new Readable({
  read: () => {}
});

slowProducer.on('readable', common.mustCall(() => {
  if (slowProducer.read(8) === null) {
    // The buffer doesn't have enough data, and the stream is not need,
    // we need to notify the reader when data arrives.
    assert.strictEqual(slowProducer._readableState.needReadable, true);
  } else {
    assert.strictEqual(slowProducer._readableState.needReadable, false);
  }
}, 4));

process.nextTick(common.mustCall(() => {
  slowProducer.push('foo');
  process.nextTick(common.mustCall(() => {
    slowProducer.push('foo');
    process.nextTick(common.mustCall(() => {
      slowProducer.push('foo');
      process.nextTick(common.mustCall(() => {
        slowProducer.push(null);
      }));
    }));
  }));
}));
