// Flags: --expose-internals
'use strict';

const common = require('../common');

if (process.features.inspector) {
  common.skip('V8 inspector is enabled');
}

const inspector = require('internal/util/inspector');

inspector.sendInspectorCommand(
  common.mustNotCall('Inspector callback should not be called'),
  common.mustCall(1),
);
