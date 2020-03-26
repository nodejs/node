// Flags: --pending-deprecation

'use strict';
const common = require('../common');

process.binding('uv');
common.expectWarning(
  'DeprecationWarning',
  'process.binding() is deprecated. Please use public APIs instead.',
  'DEP0111');
