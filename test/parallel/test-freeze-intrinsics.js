// Flags: --frozen-intrinsics
'use strict';
require('../common');
const assert = require('assert');

try {
  Object.defineProperty = 'asdf';
  assert(false);
} catch {
}
