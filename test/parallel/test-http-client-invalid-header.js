'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(function() {
  // Header value with CRLF should throw
  http.get({
    path: '/',
    headers: {
      a: 'bad value\r\n'
    }
  }, common.fail);
}, /header content for "a" contains invalid characters/);
