'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

let pushes = 0;
const rs = new stream.Readable({
  read: common.mustCall(function() {
    pushes++ < 100 ? this.push(Buffer.alloc(1024)) : this.push(null);
  }, 101)
});

const currHighWaterMark = 0;
const ws = stream.Writable({
  write: common.mustCall(function(_data, _enc, cb) {
    setImmediate(cb);
  }, 100)
});

assert.strictEqual(ws.writableHighWaterMark, 16384); // default HWM
ws.writableHighWaterMark = currHighWaterMark;
assert.strictEqual(ws.writableHighWaterMark, currHighWaterMark);

rs.pipe(ws);
