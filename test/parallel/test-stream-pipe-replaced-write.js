'use strict';

// pipe() calls dest.write(). The fast path of a flowing pipe must not go
// around a write() replaced on the instance or overridden in a subclass,
// as compression middleware does to the response it wraps.

const common = require('../common');
const assert = require('assert');
const { Readable, Writable } = require('stream');

const CHUNKS = 64;

// Pushes from inside _read(), which is what lets the fast path take over.
function source() {
  const data = Buffer.alloc(CHUNKS * 1024, 'a');
  let offset = 0;
  return new Readable({
    read() {
      if (offset >= data.length) {
        this.push(null);
        return;
      }
      this.push(data.subarray(offset, offset += 1024));
    },
  });
}

{
  const dest = new Writable({
    write(chunk, encoding, callback) {
      callback();
    },
  });
  let calls = 0;
  const write = dest.write;
  dest.write = function(chunk, encoding, callback) {
    calls++;
    return write.call(this, chunk, encoding, callback);
  };
  source().pipe(dest).on('finish', common.mustCall(() => {
    assert.strictEqual(calls, CHUNKS);
  }));
}

{
  class Counting extends Writable {
    calls = 0;

    write(chunk, encoding, callback) {
      this.calls++;
      return super.write(chunk, encoding, callback);
    }

    _write(chunk, encoding, callback) {
      callback();
    }
  }
  const dest = new Counting();
  source().pipe(dest).on('finish', common.mustCall(() => {
    assert.strictEqual(dest.calls, CHUNKS);
  }));
}
