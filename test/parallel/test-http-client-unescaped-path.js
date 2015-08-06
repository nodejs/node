'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(function() {
  // Path with spaces in it should throw.
  http.get({ path: 'bad path' }, assert.fail);
}, /contains unescaped characters/);
