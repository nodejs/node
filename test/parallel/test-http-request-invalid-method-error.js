'use strict';
const common = require('../common');
const http = require('http');

common.expectsError(
  () => http.request({ method: '\0' }),
  {
    code: 'ERR_INVALID_HTTP_TOKEN',
    type: TypeError,
    message: 'Method must be a valid HTTP token ["\u0000"]'
  }
);
