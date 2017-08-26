'use strict';
const common = require('../common');
const stream = require('stream');
const assert = require('assert');

const readable = new stream.Readable();

assert.throws(() => readable.read(), common.expectsError({
  type: Error,
  message: 'ERR_STREAM_READ_NOT_IMPLEMENTED'
}));
