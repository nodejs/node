'use strict';
var common = require('../common');
var assert = require('assert');

// Note in Windows one can only set the "user" bits.
var mask;
if (common.isWindows) {
  mask = '0600';
} else {
  mask = '0664';
}

var old = process.umask(mask);

assert.equal(parseInt(mask, 8), process.umask(old));

// confirm reading the umask does not modify it.
// 1. If the test fails, this call will succeed, but the mask will be set to 0
assert.equal(old, process.umask());
// 2. If the test fails, process.umask() will return 0
assert.equal(old, process.umask());
