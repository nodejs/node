// Flags: --expose-internals
'use strict';

const common = require('../common');
const inspector = require('internal/util/inspector');
common.skipIfInspectorDisabled();

inspector.sendInspectorCommand(
  common.mustCall(() => { throw new Error('test'); }),
  common.mustCall()
);
