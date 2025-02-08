// Flags: --expose-internals
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const inspector = require('internal/util/inspector');

inspector.sendInspectorCommand(
  common.mustNotCall('Inspector callback should not be called'),
  common.mustCall(1),
);
