'use strict';
const common = require('../common');
const stream = require('stream');

const readable = new stream.Readable();

common.expectsError(() => readable.read(), {
  code: 'ERR_STREAM_READ_NOT_IMPLEMENTED',
  type: Error,
  message: '_read() is not implemented'
});
