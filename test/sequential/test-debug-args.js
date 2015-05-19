'use strict';
// Flags: --debugger

var common = require('../common');
var assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debugger'), -1);
