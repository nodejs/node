'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(
  () => http.request({ method: '\0' }),
  {
    code: 'ERR_INVALID_HTTP_TOKEN',
    name: 'TypeError',
    message: 'Method must be a valid HTTP token ["\u0000"]'
  }
);
