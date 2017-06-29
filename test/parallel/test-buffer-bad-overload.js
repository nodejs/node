'use strict';
const common = require('../common');
const assert = require('assert');

assert.doesNotThrow(function() {
  Buffer.allocUnsafe(10);
});

const err = common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "value" argument must not be of type number. ' +
           'Received type number'
});
assert.throws(function() {
  Buffer.from(10, 'hex');
}, err);

assert.doesNotThrow(function() {
  Buffer.from('deadbeaf', 'hex');
});
