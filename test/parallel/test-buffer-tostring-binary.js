'use strict';
// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');

// Maximum length in bytes that V8 can handle.
// This equals to 268435440 bytes = (256 * 1024 * 1024) - 16
const kMaxLength = (1 << 28) - 16;

const skipMessage =
  '1..0 # Skipped: intensive toString tests due to memory confinements';
if (!common.enoughTestMem) {
  console.log(skipMessage);
  return;
}
assert(typeof gc === 'function', 'Run this test with --expose-gc');

try {
  var buf = new Buffer(kMaxLength + 1);
  // Try to allocate memory first then force gc so future allocations succeed.
  new Buffer(2 * kMaxLength);
  gc();
} catch(e) {
  // If the exception is not due to memory confinement then rethrow it.
  if (e.message !== 'Invalid array buffer length') throw (e);
  console.log(skipMessage);
  return;
}

// Ignore first byte. This should be successful.
var maxString = buf.toString('binary', 1);
assert.equal(maxString.length, kMaxLength);

// This should fail and return undefined.
maxString = buf.toString('binary');
assert.equal(maxString, undefined);
