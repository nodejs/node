'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(
  () => { http.request({method: '\0'}); },
  (error) => {
    return (error instanceof TypeError) &&
      /Method must be a valid HTTP token/.test(error);
  }
);
