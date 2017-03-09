'use strict';
// Flags: --debugger

require('../common');
const assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debugger'), -1);
