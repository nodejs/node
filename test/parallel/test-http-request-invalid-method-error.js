'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(
  () => http.request({ method: '\0' }),
  common.expectsError({
    code: 'ERR_INVALID_HTTP_TOKEN',
    type: TypeError,
    message: 'Method must be a valid HTTP token ["\u0000"]'
  })
);
