'use strict';
require('../common');
const assert = require('assert');
// minimum string size to overflow into external string space
const EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
let ucs2_control = 'a\u0000';
let write_str = 'a';


// first do basic checks
let b = new Buffer(write_str, 'ucs2');
const c = b.toString('binary');
assert.equal(b[0], 0x61);
assert.equal(b[1], 0);
assert.equal(ucs2_control, c);

// now create big strings
const size = 1 + (1 << 20);
write_str = Array(size).join(write_str);
ucs2_control = Array(size).join(ucs2_control);

// check resultant buffer and output string
b = new Buffer(write_str, 'ucs2');
// check fist Buffer created from write string
for (let i = 0; i < b.length; i += 2) {
  assert.equal(b[i], 0x61);
  assert.equal(b[i + 1], 0);
}
// create another string to create an external string
const b_bin = b.toString('binary');
const b_ucs = b.toString('ucs2');
// check control against external binary string
assert.equal(ucs2_control, b_bin);
// create buffer copy from external
const c_bin = new Buffer(b_bin, 'binary');
const c_ucs = new Buffer(b_ucs, 'ucs2');
// make sure they're the same length
assert.equal(c_bin.length, c_ucs.length);
// make sure Buffers from externals are the same
for (let i = 0; i < c_bin.length; i++) {
  assert.equal(c_bin[i], c_ucs[i]);
}
// check resultant strings
assert.equal(c_bin.toString('ucs2'), c_ucs.toString('ucs2'));
assert.equal(c_bin.toString('binary'), ucs2_control);
assert.equal(c_ucs.toString('binary'), ucs2_control);


// now let's test BASE64 and HEX ecoding/decoding
const RADIOS = 2;
const PRE_HALF_APEX = Math.ceil(EXTERN_APEX / 2) - RADIOS;
const PRE_3OF4_APEX = Math.ceil((EXTERN_APEX / 4) * 3) - RADIOS;

(function() {
  for (let j = 0; j < RADIOS * 2; j += 1) {
    const datum = b;
    const slice = datum.slice(0, PRE_HALF_APEX + j);
    const slice2 = datum.slice(0, PRE_HALF_APEX + j + 2);
    const pumped_string = slice.toString('hex');
    const pumped_string2 = slice2.toString('hex');
    const decoded = new Buffer(pumped_string, 'hex');

    // the string are the same?
    for (let k = 0; k < pumped_string.length; ++k) {
      assert.equal(pumped_string[k], pumped_string2[k]);
    }

    // the recoded buffer is the same?
    for (let i = 0; i < decoded.length; ++i) {
      assert.equal(datum[i], decoded[i]);
    }
  }
})();

(function() {
  for (let j = 0; j < RADIOS * 2; j += 1) {
    const datum = b;
    const slice = datum.slice(0, PRE_3OF4_APEX + j);
    const slice2 = datum.slice(0, PRE_3OF4_APEX + j + 2);
    const pumped_string = slice.toString('base64');
    const pumped_string2 = slice2.toString('base64');
    const decoded = new Buffer(pumped_string, 'base64');

    // the string are the same?
    for (let k = 0; k < pumped_string.length - 3; ++k) {
      assert.equal(pumped_string[k], pumped_string2[k]);
    }

    // the recoded buffer is the same?
    for (let i = 0; i < decoded.length; ++i) {
      assert.equal(datum[i], decoded[i]);
    }
  }
})();

// https://github.com/nodejs/node/issues/1024
(function() {
  const a = Array(1 << 20).join('x');
  const b = Buffer(a, 'ucs2').toString('ucs2');
  const c = Buffer(b, 'utf8').toString('utf8');

  assert.equal(a.length, b.length);
  assert.equal(b.length, c.length);

  assert.equal(a, b);
  assert.equal(b, c);
})();
