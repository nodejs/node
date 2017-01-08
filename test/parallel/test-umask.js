'use strict';
const common = require('../common');
const assert = require('assert');

// Note in Windows one can only set the "user" bits.
let mask;
if (common.isWindows) {
  mask = '0600';
} else {
  mask = '0664';
}

const old = process.umask(mask);

assert.strictEqual(parseInt(mask, 8), process.umask(old));

// confirm reading the umask does not modify it.
// 1. If the test fails, this call will succeed, but the mask will be set to 0
assert.strictEqual(old, process.umask());
// 2. If the test fails, process.umask() will return 0
assert.strictEqual(old, process.umask());
