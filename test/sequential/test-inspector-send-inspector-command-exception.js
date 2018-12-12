// Flags: --expose-internals
'use strict';

const common = require('../common');

// This is to ensure that the sendInspectorCommand function calls the cb
// function if v8_enable_inspector is enabled
process.config.variables.v8_enable_inspector = 1;
const inspector = require('internal/util/inspector');

// Pass first argument as a String instead of cb() function to trigger
// a TypeError that will be caught and onError() will be called 1 time
inspector.sendInspectorCommand(
  common.mustCall('String to trigger a TypeError'),
  common.mustCall(1)
);
