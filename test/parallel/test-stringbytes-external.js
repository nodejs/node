// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
require('../common');
const assert = require('assert');
// minimum string size to overflow into external string space
const EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
let ucs2_control = 'a\u0000';
let write_str = 'a';


// first do basic checks
let b = Buffer.from(write_str, 'ucs2');
// first check latin1
let c = b.toString('latin1');
assert.strictEqual(b[0], 0x61);
assert.strictEqual(b[1], 0);
assert.strictEqual(ucs2_control, c);
// now check binary
c = b.toString('binary');
assert.strictEqual(b[0], 0x61);
assert.strictEqual(b[1], 0);
assert.strictEqual(ucs2_control, c);

// now create big strings
const size = 1 << 20;
write_str = write_str.repeat(size);
ucs2_control = ucs2_control.repeat(size);

// check resultant buffer and output string
b = Buffer.from(write_str, 'ucs2');
// check fist Buffer created from write string
for (let i = 0; i < b.length; i += 2) {
  assert.strictEqual(b[i], 0x61);
  assert.strictEqual(b[i + 1], 0);
}

// create another string to create an external string
const b_ucs = b.toString('ucs2');

// check control against external binary string
const l_bin = b.toString('latin1');
assert.strictEqual(ucs2_control, l_bin);

// check control against external binary string
const b_bin = b.toString('binary');
assert.strictEqual(ucs2_control, b_bin);

// create buffer copy from external
const c_bin = Buffer.from(l_bin, 'latin1');
const c_ucs = Buffer.from(b_ucs, 'ucs2');
// make sure they're the same length
assert.strictEqual(c_bin.length, c_ucs.length);
// make sure Buffers from externals are the same
for (let i = 0; i < c_bin.length; i++) {
  assert.strictEqual(c_bin[i], c_ucs[i]);
}
// check resultant strings
assert.strictEqual(c_bin.toString('ucs2'), c_ucs.toString('ucs2'));
assert.strictEqual(c_bin.toString('latin1'), ucs2_control);
assert.strictEqual(c_ucs.toString('latin1'), ucs2_control);


// now let's test BASE64 and HEX ecoding/decoding
const RADIOS = 2;
const PRE_HALF_APEX = Math.ceil(EXTERN_APEX / 2) - RADIOS;
const PRE_3OF4_APEX = Math.ceil((EXTERN_APEX / 4) * 3) - RADIOS;

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
  const a = 'x'.repeat(1 << 20 - 1);
  const b = Buffer.from(a, 'ucs2').toString('ucs2');
  const c = Buffer.from(b, 'utf8').toString('utf8');

  assert.strictEqual(a.length, b.length);
  assert.strictEqual(b.length, c.length);

  assert.strictEqual(a, b);
  assert.strictEqual(b, c);
}
