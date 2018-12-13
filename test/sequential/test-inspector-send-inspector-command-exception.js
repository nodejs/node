// Flags: --expose-internals
'use strict';

const common = require('../common');

// This is to ensure that the sendInspectorCommand function calls the cb
// function if v8_enable_inspector is enabled
process.config.variables.v8_enable_inspector = 1;
const inspector = require('internal/util/inspector');

// Pass first argument as a function that throws an error that will be caught
// and onError() will be called
inspector.sendInspectorCommand(
  common.mustCall(() => { throw new Error('foo'); }),
  common.mustCall()
);
