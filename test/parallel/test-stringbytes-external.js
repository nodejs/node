'use strict';
require('../common');
var assert = require('assert');
// minimum string size to overflow into external string space
var EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
var ucs2_control = 'a\u0000';
var write_str = 'a';


// first do basic checks
var b = Buffer.from(write_str, 'ucs2');
// first check latin1
var c = b.toString('latin1');
assert.equal(b[0], 0x61);
assert.equal(b[1], 0);
assert.equal(ucs2_control, c);
// now check binary
c = b.toString('binary');
assert.equal(b[0], 0x61);
assert.equal(b[1], 0);
assert.equal(ucs2_control, c);

// now create big strings
var size = 1 + (1 << 20);
write_str = Array(size).join(write_str);
ucs2_control = Array(size).join(ucs2_control);

// check resultant buffer and output string
b = Buffer.from(write_str, 'ucs2');
// check fist Buffer created from write string
for (let i = 0; i < b.length; i += 2) {
  assert.equal(b[i], 0x61);
  assert.equal(b[i + 1], 0);
}

// create another string to create an external string
var b_ucs = b.toString('ucs2');

// check control against external binary string
var l_bin = b.toString('latin1');
assert.equal(ucs2_control, l_bin);

// check control against external binary string
var b_bin = b.toString('binary');
assert.equal(ucs2_control, b_bin);

// create buffer copy from external
var c_bin = Buffer.from(l_bin, 'latin1');
var c_ucs = Buffer.from(b_ucs, 'ucs2');
// make sure they're the same length
assert.equal(c_bin.length, c_ucs.length);
// make sure Buffers from externals are the same
for (let i = 0; i < c_bin.length; i++) {
  assert.equal(c_bin[i], c_ucs[i]);
}
// check resultant strings
assert.equal(c_bin.toString('ucs2'), c_ucs.toString('ucs2'));
assert.equal(c_bin.toString('latin1'), ucs2_control);
assert.equal(c_ucs.toString('latin1'), ucs2_control);


// now let's test BASE64 and HEX ecoding/decoding
var RADIOS = 2;
var PRE_HALF_APEX = Math.ceil(EXTERN_APEX / 2) - RADIOS;
var PRE_3OF4_APEX = Math.ceil((EXTERN_APEX / 4) * 3) - RADIOS;

{
  for (let j = 0; j < RADIOS * 2; j += 1) {
    const datum = b;
    const slice = datum.slice(0, PRE_HALF_APEX + j);
    const slice2 = datum.slice(0, PRE_HALF_APEX + j + 2);
    const pumped_string = slice.toString('hex');
    const pumped_string2 = slice2.toString('hex');
    const decoded = Buffer.from(pumped_string, 'hex');

    // the string are the same?
    for (let k = 0; k < pumped_string.length; ++k) {
      assert.strictEqual(pumped_string[k], pumped_string2[k]);
    }

    // the recoded buffer is the same?
    for (let i = 0; i < decoded.length; ++i) {
      assert.strictEqual(datum[i], decoded[i]);
    }
  }
}

{
  for (let j = 0; j < RADIOS * 2; j += 1) {
    const datum = b;
    const slice = datum.slice(0, PRE_3OF4_APEX + j);
    const slice2 = datum.slice(0, PRE_3OF4_APEX + j + 2);
    const pumped_string = slice.toString('base64');
    const pumped_string2 = slice2.toString('base64');
    const decoded = Buffer.from(pumped_string, 'base64');

    // the string are the same?
    for (let k = 0; k < pumped_string.length - 3; ++k) {
      assert.strictEqual(pumped_string[k], pumped_string2[k]);
    }

    // the recoded buffer is the same?
    for (let i = 0; i < decoded.length; ++i) {
      assert.strictEqual(datum[i], decoded[i]);
    }
  }
}

// https://github.com/nodejs/node/issues/1024
{
  const a = Array(1 << 20).join('x');
  const b = Buffer.from(a, 'ucs2').toString('ucs2');
  const c = Buffer.from(b, 'utf8').toString('utf8');

  assert.strictEqual(a.length, b.length);
  assert.strictEqual(b.length, c.length);

  assert.strictEqual(a, b);
  assert.strictEqual(b, c);
}
