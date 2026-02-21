'use strict';
const common = require('../common');
const { Readable } = require('stream');

const readable = new Readable();

readable.read();
readable.on('error', common.expectsError({
  code: 'ERR_METHOD_NOT_IMPLEMENTED',
  name: 'Error',
  message: 'The _read() method is not implemented'
}));
readable.on('close', common.mustCall());
