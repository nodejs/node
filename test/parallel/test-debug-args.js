'use strict';
// Flags: --debug-code

require('../common');
const assert = require('assert');

assert(process.execArgv.includes('--debug-code'));
