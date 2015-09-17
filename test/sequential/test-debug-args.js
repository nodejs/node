'use strict';
// Flags: --debugger

var assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debugger'), -1);
