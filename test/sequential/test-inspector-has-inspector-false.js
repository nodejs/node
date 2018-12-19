'use strict';

const common = require('../common');

// This is to ensure that the sendInspectorCommand function calls the error
// function if its called with the v8_enable_inspector is disabled

common.exposeInternals();

process.config.variables.v8_enable_inspector = 0;
const inspector = require('internal/util/inspector');

inspector.sendInspectorCommand(
  common.mustNotCall('Inspector callback should not be called'),
  common.mustCall(1),
);
