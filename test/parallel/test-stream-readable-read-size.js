'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

// Parse size as decimal integer
['1', '1.0', 1].forEach((size) => {
  const readable = new Readable({
    read: common.mustCall(),
    highWaterMark: 0,
  });
  readable.read(size);

  assert.strictEqual(readable._readableState.highWaterMark, parseInt(size, 10));
});
