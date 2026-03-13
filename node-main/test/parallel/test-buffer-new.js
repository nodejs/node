'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => new Buffer(42, 'utf8'), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "string" argument must be of type string. Received type ' +
           'number (42)'
});
