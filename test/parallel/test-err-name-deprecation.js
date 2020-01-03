'use strict';
const common = require('../common');

common.expectWarning({
  DeprecationWarning: [
    ['process.binding() is deprecated. Please use public APIs instead.',
     'DEP0111'],
    ['Directly calling process.binding(\'uv\').errname(<val>) is ' +
     'deprecated. Please use util.getSystemErrorName() instead.',
     'DEP0119']
  ]
});

process.binding('uv').errname(-1);
