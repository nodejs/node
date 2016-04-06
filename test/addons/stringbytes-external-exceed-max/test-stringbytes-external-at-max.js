'use strict';

const common = require('../../common');
const binding = require('./build/Release/binding');
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

try {
  var buf = Buffer.allocUnsafe(kStringMaxLength);
} catch (e) {
  // If the exception is not due to memory confinement then rethrow it.
  if (e.message !== 'Array buffer allocation failed') throw (e);
  console.log(skipMessage);
  return;
}

// Ensure we have enough memory available for future allocations to succeed.
if (!binding.ensureAllocation(2 * kStringMaxLength)) {
  console.log(skipMessage);
  return;
}

const maxString = buf.toString('binary');
assert.equal(maxString.length, kStringMaxLength);
