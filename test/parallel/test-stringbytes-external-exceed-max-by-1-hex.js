'use strict';

require('../common');
const assert = require('assert');

// v8 fails silently if string length > v8::String::kMaxLength
// v8::String::kMaxLength defined in v8.h
const kStringMaxLength = process.binding('buffer').kStringMaxLength;

try {
  new Buffer(kStringMaxLength * 3);
} catch(e) {
  assert.equal(e.message, 'Invalid array buffer length');
  console.log(
      '1..0 # Skipped: intensive toString tests due to memory confinements');
  return;
}

const buf = new Buffer(kStringMaxLength + 1);

assert.throws(function() {
  buf.toString('hex');
}, /toString failed/);
