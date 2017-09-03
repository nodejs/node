'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

common.expectsError(function() {
  crypto.setEngine(true);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  message: /"id"/,
});

common.expectsError(function() {
  crypto.setEngine('/path/to/engine', 'notANumber');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  message: /"flags"/,
});
