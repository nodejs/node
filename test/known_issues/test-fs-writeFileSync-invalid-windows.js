'use strict';

const common = require('../common');

// Test that using an invalid file name with writeFileSync() on Windows returns
// EINVAL. With libuv 1.x, it returns ENOTFOUND. This should be fixed when we
// update to libuv 2.x.
//
// Refs: https://github.com/nodejs/node/issues/8987

const assert = require('assert');
const fs = require('fs');

if (!common.isWindows) {
  // Change to `common.skip()` when the test is moved out of `known_issues`.
  assert.fail('Windows-only test');
}

assert.throws(() => {
  fs.writeFileSync('fhqwhgads??', 'come on');
}, { code: 'EINVAL' });
