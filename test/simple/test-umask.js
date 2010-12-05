var common = require('../common');
var assert = require('assert');

var mask = 0664;
var old = process.umask(mask);

assert.equal(mask, process.umask(old));

// confirm reading the umask does not modify it.
// 1. If the test fails, this call will succeed, but the mask will be set to 0
assert.equal(old, process.umask());
// 2. If the test fails, process.umask() will return 0
assert.equal(old, process.umask());
