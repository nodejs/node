'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

const rs = new stream.Readable({
  read: function() {
    this.push(Buffer.alloc(1024));
    this.push(null);
  }
});

const ws = stream.Writable({ write() {
  this.writableHighWaterMark = 20;
} });

ws.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_STREAM_UPDATING_HIGHWATERMARK_IN_WRITE');
  assert.strictEqual(err.message, 'Cannot update highwatermark while writing');
}));
ws.on('end', common.mustNotCall());

rs.pipe(ws);
