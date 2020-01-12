'use strict';
const common = require('../common');

process.binding('uv').errname(-1);
common.expectWarning(
  'DeprecationWarning',
  'Directly calling process.binding(\'uv\').errname(<val>) is ' +
  'deprecated. Please use util.getSystemErrorName() instead.',
  'DEP0119');
