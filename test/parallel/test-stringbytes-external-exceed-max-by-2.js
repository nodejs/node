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

const buf2 = new Buffer(kStringMaxLength + 2);

const maxString = buf2.toString('utf16le');
assert.equal(maxString.length, (kStringMaxLength + 2) / 2);
