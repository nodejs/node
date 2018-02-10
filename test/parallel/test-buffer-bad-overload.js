'use strict';
const common = require('../common');
const assert = require('assert');

Buffer.allocUnsafe(10); // Should not throw.

const err = common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "value" argument must not be of type number. ' +
           'Received type number'
});
assert.throws(function() {
  Buffer.from(10, 'hex');
}, err);

Buffer.from('deadbeaf', 'hex'); // Should not throw.
