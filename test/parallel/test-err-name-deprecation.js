'use strict';
const common = require('../common');

if (!process.execArgv.includes('--pending-deprecation'))
  common.requireFlags(['--pending-deprecation']);

common.expectWarning(
  'DeprecationWarning',
  'Directly calling process.binding(\'uv\').errname(<val>) is being ' +
  'deprecated. Please make sure to use util.getSystemErrorName() instead.',
  'DEP0119'
);

process.binding('uv').errname(-1);
