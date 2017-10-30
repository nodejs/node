'use strict';

const common = require('../common');
const assert = require('assert');

assert.throws(() => new Buffer(42, 'utf8'), common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "string" argument must be of type string. Received type number'
}));
