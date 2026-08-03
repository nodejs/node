// Flags: --pending-deprecation

'use strict';
const common = require('../common');

common.expectWarning(
  'DeprecationWarning',
  'module.parent is deprecated due to accuracy issues. Please use ' +
    'require.main to find program entry point instead.',
  'DEP0144'
);

module.parent = undefined;
