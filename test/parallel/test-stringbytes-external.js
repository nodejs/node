'use strict';
var common = require('../common');
var assert = require('assert');
// minimum string size to overflow into external string space
var EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
var ucs2_control = 'a\u0000';
var write_str = 'a';


// first do basic checks
var b = new Buffer(write_str, 'ucs2');
var c = b.toString('binary');
assert.equal(b[0], 0x61);
assert.equal(b[1], 0);
assert.equal(ucs2_control, c);

// now create big strings
var size = 1 + (1 << 20);
write_str = Array(size).join(write_str);
ucs2_control = Array(size).join(ucs2_control);

// check resultant buffer and output string
var b = new Buffer(write_str, 'ucs2');
// check fist Buffer created from write string
for (var i = 0; i < b.length; i += 2) {
  assert.equal(b[i], 0x61);
  assert.equal(b[i + 1], 0);
}
// create another string to create an external string
var b_bin = b.toString('binary');
var b_ucs = b.toString('ucs2');
// check control against external binary string
assert.equal(ucs2_control, b_bin);
// create buffer copy from external
var c_bin = new Buffer(b_bin, 'binary');
var c_ucs = new Buffer(b_ucs, 'ucs2');
// make sure they're the same length
assert.equal(c_bin.length, c_ucs.length);
// make sure Buffers from externals are the same
for (var i = 0; i < c_bin.length; i++) {
  assert.equal(c_bin[i], c_ucs[i]);
}
// check resultant strings
assert.equal(c_bin.toString('ucs2'), c_ucs.toString('ucs2'));
assert.equal(c_bin.toString('binary'), ucs2_control);
assert.equal(c_ucs.toString('binary'), ucs2_control);


// now let's test BASE64 and HEX ecoding/decoding
var RADIOS = 2;
var PRE_HALF_APEX = Math.ceil(EXTERN_APEX / 2) - RADIOS;
var PRE_3OF4_APEX = Math.ceil((EXTERN_APEX / 4) * 3) - RADIOS;

(function() {
  for (var j = 0; j < RADIOS * 2; j += 1) {
    var datum = b;
    var slice = datum.slice(0, PRE_HALF_APEX + j);
    var slice2 = datum.slice(0, PRE_HALF_APEX + j + 2);
    var pumped_string = slice.toString('hex');
    var pumped_string2 = slice2.toString('hex');
    var decoded = new Buffer(pumped_string, 'hex');

    // the string are the same?
    for (var k = 0; k < pumped_string.length; ++k) {
      assert.equal(pumped_string[k], pumped_string2[k]);
    }

    // the recoded buffer is the same?
    for (var i = 0; i < decoded.length; ++i) {
      assert.equal(datum[i], decoded[i]);
    }
  }
})();

(function() {
  for (var j = 0; j < RADIOS * 2; j += 1) {
    var datum = b;
    var slice = datum.slice(0, PRE_3OF4_APEX + j);
    var slice2 = datum.slice(0, PRE_3OF4_APEX + j + 2);
    var pumped_string = slice.toString('base64');
    var pumped_string2 = slice2.toString('base64');
    var decoded = new Buffer(pumped_string, 'base64');

    // the string are the same?
    for (var k = 0; k < pumped_string.length - 3; ++k) {
      assert.equal(pumped_string[k], pumped_string2[k]);
    }

    // the recoded buffer is the same?
    for (var i = 0; i < decoded.length; ++i) {
      assert.equal(datum[i], decoded[i]);
    }
  }
})();

// https://github.com/nodejs/node/issues/1024
(function() {
  var a = Array(1 << 20).join('x');
  var b = Buffer(a, 'ucs2').toString('ucs2');
  var c = Buffer(b, 'utf8').toString('utf8');

  assert.equal(a.length, b.length);
  assert.equal(b.length, c.length);

  assert.equal(a, b);
  assert.equal(b, c);
})();

// v8 fails silently if string length > v8::String::kMaxLength
(function() {
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

  const buf0 = new Buffer(kStringMaxLength * 2 + 2);
  const buf1 = buf0.slice(0, kStringMaxLength + 1);
  const buf2 = buf0.slice(0, kStringMaxLength);
  const buf3 = buf0.slice(0, kStringMaxLength + 2);

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
    buf0.toString('utf16le');
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

  var maxString = buf2.toString();
  assert.equal(maxString.length, kStringMaxLength);
  // Free the memory early instead of at the end of the next assignment
  maxString = undefined;

  maxString = buf2.toString('binary');
  assert.equal(maxString.length, kStringMaxLength);
  maxString = undefined;

  maxString = buf1.toString('binary', 1);
  assert.equal(maxString.length, kStringMaxLength);
  maxString = undefined;

  maxString = buf1.toString('binary', 0, kStringMaxLength);
  assert.equal(maxString.length, kStringMaxLength);
  maxString = undefined;

  maxString = buf3.toString('utf16le');
  assert.equal(maxString.length, (kStringMaxLength + 2) / 2);
  maxString = undefined;
})();
