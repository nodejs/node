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

const buf1 = new Buffer(kStringMaxLength + 1);

assert.throws(function() {
  buf1.toString();
}, /toString failed|Invalid array buffer length/);

assert.throws(function() {
  buf1.toString('ascii');
}, /toString failed/);

assert.throws(function() {
  buf1.toString('utf8');
}, /toString failed/);

assert.throws(function() {
  buf1.toString('binary');
}, /toString failed/);

assert.throws(function() {
  buf1.toString('base64');
}, /toString failed/);

assert.throws(function() {
  buf1.toString('hex');
}, /toString failed/);

var maxString = buf1.toString('binary', 1);
assert.equal(maxString.length, kStringMaxLength);
maxString = undefined;

maxString = buf1.toString('binary', 0, kStringMaxLength);
assert.equal(maxString.length, kStringMaxLength);
// Free the memory early instead of at the end of the next assignment
maxString = undefined;
