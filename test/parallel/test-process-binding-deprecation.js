// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');

common.expectWarning(
  'DeprecationWarning',
  'Use of process.binding(\'uv\') is deprecated.',
  'DEP0111'
);

assert(process.binding('uv'));
