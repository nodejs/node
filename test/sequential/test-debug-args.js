'use strict';
// Flags: --debug-code

require('../common');
var assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debug-code'), -1);
