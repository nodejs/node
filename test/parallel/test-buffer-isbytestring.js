'use strict';

require('../common');
const assert = require('assert');
const { isByteString } = require('buffer');

function reference(str) {
  for (let i = 0; i < str.length; i++) {
    if (str.charCodeAt(i) > 0xFF) return false;
  }
  return true;
}

// Basic cases.
assert.strictEqual(isByteString(''), true);
assert.strictEqual(isByteString('hello'), true);
assert.strictEqual(isByteString('\x00'), true);
assert.strictEqual(isByteString('\x7f\x80'), true);
assert.strictEqual(isByteString('caf\u00e9'), true);
assert.strictEqual(isByteString('\u00ff'), true);
assert.strictEqual(isByteString('\u0100'), false);
assert.strictEqual(isByteString('\u20ac'), false);
assert.strictEqual(isByteString('\uffff'), false);
// Surrogate pairs and lone surrogates are > 0xFF.
assert.strictEqual(isByteString('\ud83d\ude00'), false);
assert.strictEqual(isByteString('\ud800'), false);
assert.strictEqual(isByteString('\udfff'), false);

// Position of the offending code unit must not matter, and long strings must
// exercise the vectorized paths.
for (const len of [1, 7, 8, 15, 16, 31, 32, 33, 63, 64, 65, 1000, 4099]) {
  const base = 'a\u00ff'.repeat(len).slice(0, len);
  assert.strictEqual(isByteString(base), true);
  for (const pos of [0, len >> 1, len - 1]) {
    for (const ch of ['\u0100', '\u1234', '\ud800', '\uffff']) {
      const str = base.slice(0, pos) + ch + base.slice(pos + 1);
      assert.strictEqual(isByteString(str), false, `len=${len} pos=${pos}`);
    }
  }
}

// Strings stored with a two-byte representation that only contain code units
// <= 0xFF must still be reported as ByteStrings.
{
  const twoByte = '\u0100' + 'abc\u00e9\u00ff'.repeat(100);
  const sliced = twoByte.slice(1);
  assert.strictEqual(isByteString(twoByte), false);
  assert.strictEqual(isByteString(sliced), true);
  assert.strictEqual(isByteString(twoByte.substring(1, 20)), true);
}

// Cons strings (results of concatenation) with mixed representations.
{
  let cons = '';
  for (let i = 0; i < 100; i++) cons += `x${i}\u00e9`;
  assert.strictEqual(isByteString(cons), true);
  assert.strictEqual(isByteString(cons + '\u0100'), false);
  assert.strictEqual(isByteString('\u0100' + cons), false);
  assert.strictEqual(isByteString(cons + '\u0100'.slice(1) + cons), true);
}

// Randomized comparison with the reference implementation.
for (let i = 0; i < 1000; i++) {
  const len = Math.floor(Math.random() * 100);
  const max = Math.random() < 0.5 ? 0x100 : 0x10000;
  let str = '';
  for (let j = 0; j < len; j++) {
    // Keep the probability of producing a code unit > 0xFF low so that
    // both outcomes are exercised.
    const code = Math.random() < 0.98 ?
      Math.floor(Math.random() * 0x100) :
      Math.floor(Math.random() * max);
    str += String.fromCharCode(code);
  }
  assert.strictEqual(isByteString(str), reference(str), JSON.stringify(str));
}

// Invalid argument types.
[
  undefined, null, 1, 1n, true, {}, [], Symbol('a'),
  Buffer.from('a'), new Uint8Array(1), new ArrayBuffer(1),
  new String('a'),
].forEach((input) => {
  assert.throws(() => isByteString(input), { code: 'ERR_INVALID_ARG_TYPE' });
});
