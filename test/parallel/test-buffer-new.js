'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => new Buffer(42, 'utf8'), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});
