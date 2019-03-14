// Flags: --frozen-intrinsics
'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => Object.defineProperty = 'asdf',
  TypeError
);
