// Flags: --expose-internals
'use strict';

const common = require('../common');

// This is to ensure that the sendInspectorCommand function calls the error
// function if its called with the v8_enable_inspector is disabled

const v8EnableInspectorBk = process.config.variables.v8_enable_inspector;
process.config.variables.v8_enable_inspector = 0;
const inspector = require('internal/util/inspector');

function restoreProcessVar() {
  process.config.variables.v8_enable_inspector = v8EnableInspectorBk;
}

inspector.sendInspectorCommand(
  common.mustNotCall('Inspector callback should not be called'),
  common.mustCall(restoreProcessVar, 1),
);
