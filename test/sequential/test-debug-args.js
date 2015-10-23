'use strict';
// Flags: --debug-code

var common = require('../common');
var assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debug-code'), -1);
