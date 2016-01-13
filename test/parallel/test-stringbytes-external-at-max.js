'use strict';
// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');

// v8 fails silently if string length > v8::String::kMaxLength
// v8::String::kMaxLength defined in v8.h
const kStringMaxLength = process.binding('buffer').kStringMaxLength;

const skipMessage =
  '1..0 # Skipped: intensive toString tests due to memory confinements';
if (!common.enoughTestMem) {
  console.log(skipMessage);
  return;
}
assert(typeof gc === 'function', 'Run this test with --expose-gc');

try {
  var buf = new Buffer(kStringMaxLength);
  // Try to allocate memory first then force gc so future allocations succeed.
  new Buffer(2 * kStringMaxLength);
  gc();
} catch (e) {
  // If the exception is not due to memory confinement then rethrow it.
  if (e.message !== 'Invalid array buffer length') throw (e);
  console.log(skipMessage);
  return;
}

const maxString = buf.toString('binary');
assert.equal(maxString.length, kStringMaxLength);
