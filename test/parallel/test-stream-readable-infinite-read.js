'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

const buf = Buffer.alloc(8192);

const readable = new Readable({
  highWaterMark: 16 * 1024,
  read: common.mustCall(function() {
    this.push(buf);
  }, 12)
});

let i = 0;

readable.on('readable', common.mustCall(function() {
  if (i++ === 10) {
    readable.removeAllListeners('readable');
    return;
  }

  assert.strictEqual(readable.read().length, 8192);
}, 11));
