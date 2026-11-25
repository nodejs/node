'use strict';

// A flowing Readable whose _read() pushes EOF in the call after its last
// chunk emits 'end' even if the consumer pauses on that chunk. pipe() then
// ends a destination whose last write returned false without waiting for
// 'drain'.

const common = require('../common');
const assert = require('assert');
const { Readable, Writable } = require('stream');

function source() {
  const chunks = [Buffer.alloc(64), Buffer.alloc(64), Buffer.alloc(64)];
  return new Readable({
    highWaterMark: 64,
    read() {
      this.push(chunks.shift() ?? null);
    },
  });
}

// pause() from the 'data' listener of the last chunk.
{
  const src = source();
  let received = 0;
  src.on('data', common.mustCall(() => {
    if (++received === 3)
      src.pause();
  }, 3));
  src.on('end', common.mustCall());
}

// pipe() to a destination that never completes its last write.
{
  const src = source();
  let written = 0;
  const dest = new Writable({
    highWaterMark: 1,
    write: common.mustCall((chunk, encoding, cb) => {
      if (++written < 3)
        setImmediate(cb);
    }, 3),
  });
  src.pipe(dest);
  src.on('end', common.mustCall(() => {
    assert.strictEqual(dest.writableEnded, true);
  }));
}
