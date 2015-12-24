'use strict';
// Flags: --debugger

require('../common');
var assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debugger'), -1);
