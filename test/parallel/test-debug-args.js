'use strict';
// Flags: --debug-code

const common = require('../common');
const assert = require('assert');

if (!common.isMainThread)
  common.skip('execArgv does not affect Workers');

assert(process.execArgv.includes('--debug-code'));
