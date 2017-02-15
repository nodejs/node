'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(
  () => { http.request({method: '\0'}); },
  common.expectsError(undefined, TypeError, 'Method must be a valid HTTP token')
);
