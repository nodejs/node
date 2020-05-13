'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

let currHighWaterMark = 20;
let pushes = 0;
const rs = new stream.Readable({
  read: common.mustCall(function() {
    if (pushes++ === 100) {
      this.push(null);
      return;
    }

    const highWaterMark = this.readableHighWaterMark;

    // Both highwatermark and objectmode should update
    // before it makes next _read request.
    assert.strictEqual(highWaterMark, currHighWaterMark);

    this.push(Buffer.alloc(1024));

    if (pushes === 30) {
      this.readableHighWaterMark = 2048;
      currHighWaterMark = 2048;
    }

  }, 101)
});

const ws = stream.Writable({
  write: common.mustCall(function(data, enc, cb) {
    setImmediate(cb);
  }, 100)
});

assert.strictEqual(rs.readableHighWaterMark, 16384); // default HWM
rs.readableHighWaterMark = currHighWaterMark;
assert.strictEqual(rs.readableHighWaterMark, currHighWaterMark);

rs.pipe(ws);
