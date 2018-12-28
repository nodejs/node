'use strict';

// Test that V8 options not implemented by Node.js are passed as-is to the
// V8 engine and do not result in an error just because they are unreocgnized
// by Node.js.

const common = require('../common');

if (!common.isMainThread)
  common.skip('execArgv does not affect Workers');

common.requireFlags('--debug-code');
