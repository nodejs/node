'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

let pushes = 0;
const rs = new stream.Readable({
  read: common.mustCall(function() {
    if (pushes++ === 100) {
      this.push(null);
      return;
    }

    this.push(Buffer.alloc(1024));
  }, 101)
});

let currHighWaterMark = 0;
let write = 0;
const ws = stream.Writable({
  write: common.mustCall(function(data, enc, cb) {

    const highWaterMark = this.writableHighWaterMark;

    // Should update highwatermark
    // after emptying current buffer
    assert.strictEqual(highWaterMark, currHighWaterMark);

    if (write++ === 30) {
      this.writableHighWaterMark = 2048;
      currHighWaterMark = 2048;
    }

    setImmediate(cb);

  }, 100)
});

assert.strictEqual(ws.writableHighWaterMark, 16384); // default HWM
ws.writableHighWaterMark = currHighWaterMark;
assert.strictEqual(ws.writableHighWaterMark, currHighWaterMark);

rs.pipe(ws);
