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
let currObjectMode = true;
let read = 0;
const ws = stream.Writable({
  highWaterMark: currHighWaterMark,
  objectMode: currObjectMode,
  write: common.mustCall(function(data, enc, cb) {

    const highWaterMark = this._writableState.highWaterMark;
    const objectMode = this._writableState.objectMode;

    // Should update highwatermark and objectmode
    // after emptying current buffer
    assert.strictEqual(highWaterMark, currHighWaterMark);
    assert.strictEqual(objectMode, currObjectMode);

    if (read++ === 30) {
      this.updateWritableHighwaterMark(2048);
      this.updateWritableObjectMode(false);
      currHighWaterMark = 2048;
      currObjectMode = false;
    }

    setImmediate(cb);

  }, 100)
});

rs.pipe(ws);
