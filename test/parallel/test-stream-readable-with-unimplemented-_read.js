'use strict';
require('../common');

const assert = require('assert');
const { Readable } = require('stream');

const readable = new Readable();

assert.throws(
  () => {
    readable.read();
  },
  {
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
    name: 'Error',
    message: 'The _read() method is not implemented'
  }
);
