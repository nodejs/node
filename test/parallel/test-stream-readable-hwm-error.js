'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

const rs = new stream.Readable({
  read: function() {
    this.readableHighWaterMark = 200;
    this.push(null);
  }
});

const ws = stream.Writable({ write() {} });

rs.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_STREAM_UPDATING_HIGHWATERMARK_IN_READ');
  assert.strictEqual(err.message, 'Cannot update highwatermark while reading');
}));
rs.on('end', common.mustNotCall());

rs.pipe(ws);
