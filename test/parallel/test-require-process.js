'use strict';
var assert = require('assert');

var nativeProcess = require('process');
assert.strictEqual(nativeProcess, process,
  'require("process") should return a reference to global process');
