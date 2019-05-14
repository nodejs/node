'use strict';
const common = require('../common');

// Flags: --pending-deprecation

common.expectWarning({
  DeprecationWarning: [
    ['process.binding() is deprecated. Please use public APIs instead.',
     'DEP0111'],
    ['Directly calling process.binding(\'uv\').errname(<val>) is being ' +
     'deprecated. Please make sure to use util.getSystemErrorName() instead.',
     'DEP0119']
  ]
});

process.binding('uv').errname(-1);
