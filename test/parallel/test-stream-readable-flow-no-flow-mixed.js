'use strict';

const common = require('../common');

// This test ensures that Readable stream switches between flowing and
// non-flowing state properly when varying the 'readable' and 'data' event
// subscription.

const assert = require('assert');
const { Readable } = require('stream');

const flowingData = [
  { value: 'a' },
  { value: 'b' },
  { value: 'c', subscribeData: true },
  { value: 'd' },
  { value: 'e' },
  { value: 'f', removeReadable: true },
  { value: 'g' },
  { value: 'h' },
  { value: 'i', subscribeReadable: true },
  null,
];

const r = new Readable({
  read: common.mustCall(() => {
    process.nextTick(() => {
      r.push(flowingData.shift());
    });
  }, flowingData.length),
  objectMode: true,

  // The water mark shouldn't matter but we'll want to ensure the stream won't
  // buffer data before we have a chance to react to the subscribe/unsubscribe
  // event controls.
  highWaterMark: 0,
});

// Store data received through 'readable' events and 'data' events.
const actualReadable = [];
const actualData = [];

r.on('end', common.mustCall(() => {
  assert.deepStrictEqual(actualReadable, ['a', 'b', 'c', 'd', 'e', 'f']);
  assert.deepStrictEqual(actualData, ['d', 'e', 'f', 'g', 'h', 'i']);
}));

// Subscribing 'readable' should set flowing state to false.
assert.strictEqual(r.readableFlowing, null);
r.on('readable', common.mustCall(() => {
  const v = r.read();
  actualReadable.push(v.value);

  if (v.subscribeData) {

    // Subsribing 'data' should not change flowing state.
    assert.strictEqual(r.readableFlowing, false);
    r.on('data', common.mustCall((data) => {
      actualData.push(data.value);

      if (data.subscribeReadable) {

        // Re-subsribing readable should put the stream back to non-flowing
        // state.
        assert.strictEqual(r.readableFlowing, true);
        r.on('readable', common.mustCall(() => {
          // The stream is at the end, but 'readable' is signaled without the
          // stream knowing this. The 'r.read()' here will result in _read
          // getting executed, which will then push the final null.
          //
          // NOTE: The 'null' here signals non-synchronous read. It is NOT the
          // same 'null' that the _read ends up pushing to signal end of
          // stream.
          assert.strictEqual(r.read(), null);
        }));
        assert.strictEqual(r.readableFlowing, false);
      }
    }, 6));
    assert.strictEqual(r.readableFlowing, false);
  }

  if (v.removeReadable) {
    // Removing 'readable' should allow the stream to flow into 'data' without
    // us calling 'read()' manually.
    //
    // This should also cahgne the flowing state - although it is delayed into
    // the next tick (within removeAllListeners).
    assert.strictEqual(r.readableFlowing, false);
    r.removeAllListeners('readable');
    process.nextTick(() => {
      assert.strictEqual(r.readableFlowing, true);
    });
  } else {
    // We'll need to call r.read() to trigger the next read.
    //
    // It should return 'null' as the actual _read implementation is
    // asynchronous but we still need to call it to trigger the push on
    // next tick.
    assert.strictEqual(r.read(), null);
  }
}, 6));
assert.strictEqual(r.readableFlowing, false);
