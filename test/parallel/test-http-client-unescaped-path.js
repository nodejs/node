'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

assert.throws(function() {
  // Path with spaces in it should throw.
  http.get({ path: 'bad path' }, common.fail);
}, /contains unescaped characters/);
