'use strict';
// Flags: --debugger

const common = require('../common');
const assert = require('assert');

assert.notEqual(process.execArgv.indexOf('--debugger'), -1);
