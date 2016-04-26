'use strict';
require('../common');
var assert = require('assert');

var nativeProcess = require('process');
assert.strictEqual(nativeProcess, process,
                   'require("process") should return global process reference');
