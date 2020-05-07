'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

let currHighWaterMark = 20;
let currObjectMode = true;
let pushes = 0;
const rs = new stream.Readable({
  highWaterMark: currHighWaterMark,
  objectMode: currObjectMode,
  read: common.mustCall(function() {
    if (pushes++ === 100) {
      this.push(null);
      return;
    }

    const highWaterMark = this._readableState.highWaterMark;
    const objectMode = this._readableState.objectMode;

    // Both highwatermark and objectmode should update
    // before it makes next _read request.
    assert(highWaterMark === currHighWaterMark);
    assert(objectMode === currObjectMode);

    this.push(Buffer.alloc(1024));

    if (pushes === 30) {
        this.updateReadableHighwaterMark(2048);
        this.updateReadableObjectMode(false);
        currHighWaterMark = 2048;
        currObjectMode = false;
    }

  }, 101)
});

const ws = stream.Writable({
  write: common.mustCall(function(data, enc, cb) {
    setImmediate(cb);
  }, 100)
});

rs.pipe(ws);