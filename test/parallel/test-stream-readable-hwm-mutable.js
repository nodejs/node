'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

const currHighWaterMark = 20;
let pushes = 0;
const rs = new stream.Readable({
  read: common.mustCall(function() {
    (pushes++ < 100) ? this.push(Buffer.alloc(1024)) : this.push(null);
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
