'use strict';

const common = require('../common');

common.expectsError(() => new Buffer(42, 'utf8'), {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "string" argument must be of type string. Received type number'
});
