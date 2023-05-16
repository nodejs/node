'use strict';
require('../common');
const assert = require('assert');
const { Writable } = require('stream');

// Test interaction between calling .destroy() on a writable and pending
// writes.

for (const withPendingData of [ false, true ]) {
  for (const useEnd of [ false, true ]) {
    const callbacks = [];

    const w = new Writable({
      write(data, enc, cb) {
        callbacks.push(cb);
      },
      // Effectively disable the HWM to observe 'drain' events more easily.
      highWaterMark: 1
    });

    let chunksWritten = 0;
    let drains = 0;
    w.on('drain', () => drains++);

    function onWrite(err) {
      if (err) {
        assert.strictEqual(w.destroyed, true);
        assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
      } else {
        chunksWritten++;
      }
    }

    w.write('abc', onWrite);
    assert.strictEqual(chunksWritten, 0);
    assert.strictEqual(drains, 0);
    callbacks.shift()();
    assert.strictEqual(chunksWritten, 1);
    assert.strictEqual(drains, 1);

    if (withPendingData) {
      // Test 2 cases: There either is or is not data still in the write queue.
      // (The second write will never actually get executed either way.)
      w.write('def', onWrite);
    }
    if (useEnd) {
      // Again, test 2 cases: Either we indicate that we want to end the
      // writable or not.
      w.end('ghi', onWrite);
    } else {
      w.write('ghi', onWrite);
    }

    assert.strictEqual(chunksWritten, 1);
    w.destroy();
    assert.strictEqual(chunksWritten, 1);
    callbacks.shift()();
    assert.strictEqual(chunksWritten, useEnd && !withPendingData ? 1 : 2);
    assert.strictEqual(callbacks.length, 0);
    assert.strictEqual(drains, 1);
  }
}
