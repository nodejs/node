'use strict';
const common = require('../common');
const stream = require('stream');
const assert = require('assert');

const readable = new stream.Readable();

assert.throws(() => readable.read(), common.expectsError({
  code: 'ERR_STREAM_READ_NOT_IMPLEMENTED',
  type: Error,
  message: '_read() is not implemented'
}));
