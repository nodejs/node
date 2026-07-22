'use strict';

const common = require('../common');
const assert = require('assert');
const { Buffer } = require('buffer');
const vm = require('vm');

[
  [32, 'latin1'],
  [NaN, 'utf8'],
  [{}, 'latin1'],
  [],
].forEach((args) => {
  assert.throws(
    () => Buffer.byteLength(...args),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "string" argument must be of type string or an instance ' +
               'of Buffer or ArrayBuffer.' +
               common.invalidArgTypeHelper(args[0])
    }
  );
});

assert(ArrayBuffer.isView(new Buffer(10)));
assert(ArrayBuffer.isView(Buffer.alloc(10)));
assert(ArrayBuffer.isView(Buffer.allocUnsafe(10)));
assert(ArrayBuffer.isView(Buffer.allocUnsafeSlow(10)));
assert(ArrayBuffer.isView(Buffer.from('')));

// buffer
const incomplete = Buffer.from([0xe4, 0xb8, 0xad, 0xe6, 0x96]);
assert.strictEqual(Buffer.byteLength(incomplete), 5);
const ascii = Buffer.from('abc');
assert.strictEqual(Buffer.byteLength(ascii), 3);

// ArrayBuffer
const buffer = new ArrayBuffer(8);
assert.strictEqual(Buffer.byteLength(buffer), 8);

// TypedArray
const int8 = new Int8Array(8);
assert.strictEqual(Buffer.byteLength(int8), 8);
const uint8 = new Uint8Array(8);
assert.strictEqual(Buffer.byteLength(uint8), 8);
const uintc8 = new Uint8ClampedArray(2);
assert.strictEqual(Buffer.byteLength(uintc8), 2);
const int16 = new Int16Array(8);
assert.strictEqual(Buffer.byteLength(int16), 16);
const uint16 = new Uint16Array(8);
assert.strictEqual(Buffer.byteLength(uint16), 16);
const int32 = new Int32Array(8);
assert.strictEqual(Buffer.byteLength(int32), 32);
const uint32 = new Uint32Array(8);
assert.strictEqual(Buffer.byteLength(uint32), 32);
const float32 = new Float32Array(8);
assert.strictEqual(Buffer.byteLength(float32), 32);
const float64 = new Float64Array(8);
assert.strictEqual(Buffer.byteLength(float64), 64);

// DataView
const dv = new DataView(new ArrayBuffer(2));
assert.strictEqual(Buffer.byteLength(dv), 2);

// Special case: zero length string
assert.strictEqual(Buffer.byteLength('', 'ascii'), 0);
assert.strictEqual(Buffer.byteLength('', 'HeX'), 0);

// utf8
assert.strictEqual(Buffer.byteLength('∑éllö wørl∂!', 'utf-8'), 19);
assert.strictEqual(Buffer.byteLength('κλμνξο', 'utf8'), 12);
assert.strictEqual(Buffer.byteLength('挵挶挷挸挹', 'utf-8'), 15);
assert.strictEqual(Buffer.byteLength('𠝹𠱓𠱸', 'UTF8'), 12);
// Without an encoding, utf8 should be assumed
assert.strictEqual(Buffer.byteLength('hey there'), 9);
assert.strictEqual(Buffer.byteLength('𠱸挶νξ#xx :)'), 17);
assert.strictEqual(Buffer.byteLength('hello world', ''), 11);
// It should also be assumed with unrecognized encoding
assert.strictEqual(Buffer.byteLength('hello world', 'abc'), 11);
assert.strictEqual(Buffer.byteLength('ßœ∑≈', 'unkn0wn enc0ding'), 10);

// base64
assert.strictEqual(Buffer.byteLength('aGVsbG8gd29ybGQ=', 'base64'), 11);
assert.strictEqual(Buffer.byteLength('aGVsbG8gd29ybGQ=', 'BASE64'), 11);
assert.strictEqual(Buffer.byteLength('bm9kZS5qcyByb2NrcyE=', 'base64'), 14);
assert.strictEqual(Buffer.byteLength('aGkk', 'base64'), 3);
assert.strictEqual(
  Buffer.byteLength('bHNrZGZsa3NqZmtsc2xrZmFqc2RsZmtqcw==', 'base64'), 25
);
// base64url
assert.strictEqual(Buffer.byteLength('aGVsbG8gd29ybGQ', 'base64url'), 11);
assert.strictEqual(Buffer.byteLength('aGVsbG8gd29ybGQ', 'BASE64URL'), 11);
assert.strictEqual(Buffer.byteLength('bm9kZS5qcyByb2NrcyE', 'base64url'), 14);
assert.strictEqual(Buffer.byteLength('aGkk', 'base64url'), 3);
assert.strictEqual(
  Buffer.byteLength('bHNrZGZsa3NqZmtsc2xrZmFqc2RsZmtqcw', 'base64url'), 25
);
// special padding
assert.strictEqual(Buffer.byteLength('aaa=', 'base64'), 2);
assert.strictEqual(Buffer.byteLength('aaaa==', 'base64'), 3);
assert.strictEqual(Buffer.byteLength('aaa=', 'base64url'), 2);
assert.strictEqual(Buffer.byteLength('aaaa==', 'base64url'), 3);

assert.strictEqual(Buffer.byteLength('Il était tué'), 14);
assert.strictEqual(Buffer.byteLength('Il était tué', 'utf8'), 14);

for (const encoding of ['ascii', 'latin1', 'binary'].flatMap((e) => [e, e.toUpperCase()])) {
  assert.strictEqual(Buffer.byteLength('Il était tué', encoding), 12);
}

for (const encoding of ['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].flatMap((e) => [e, e.toUpperCase()])) {
  assert.strictEqual(Buffer.byteLength('Il était tué', encoding), 24);
}

// Test that ArrayBuffer from a different context is detected correctly
const arrayBuf = vm.runInNewContext('new ArrayBuffer()');
assert.strictEqual(Buffer.byteLength(arrayBuf), 0);

// Verify that invalid encodings are treated as utf8
for (let i = 1; i < 10; i++) {
  const encoding = String(i).repeat(i);

  assert.ok(!Buffer.isEncoding(encoding));
  assert.strictEqual(Buffer.byteLength('foo', encoding),
                     Buffer.byteLength('foo', 'utf8'));
}

// byteLength('utf8') must equal the bytes Buffer.from writes, including
// unpaired surrogates (3-byte U+FFFD). Large inputs exercise the SIMD path.
{
  const HI = '\uD83D'; // Unpaired high surrogate
  const LO = '\uDE00'; // Unpaired low surrogate
  const PAIR = '\u{1F600}'; // Valid surrogate pair (4 bytes)
  const enc = new TextEncoder();

  // Independent UTF-8 byte-length oracle with replacement semantics.
  const utf8Bytes = (str) => {
    let n = 0;
    for (let i = 0; i < str.length; i++) {
      const c = str.charCodeAt(i);
      if (c < 0x80) n += 1;
      else if (c < 0x800) n += 2;
      else if (c >= 0xD800 && c <= 0xDBFF) {
        const next = str.charCodeAt(i + 1);
        if (next >= 0xDC00 && next <= 0xDFFF) { n += 4; i++; } else n += 3;
      } else n += 3; // BMP >= 0x800, incl. unpaired low surrogate
    }
    return n;
  };

  const cases = [
    'a'.repeat(300) + HI + 'b'.repeat(300), // Unpaired high inside large 2-byte
    'a'.repeat(300) + LO + 'b'.repeat(300), // Unpaired low inside large 2-byte
    HI.repeat(200), // many unpaired highs
    (HI + LO).repeat(200), // Reversed order, still unpaired
    PAIR.repeat(200), // valid pairs, large
    '中'.repeat(500), // BMP 3-byte, large
    'é'.repeat(500), // latin1, large
    `a中${PAIR}${HI}é`.repeat(100), // mixed, large
    `A${HI}`, // Tiny, below SIMD threshold
    HI, // Single unpaired surrogate
    '',
  ];

  for (const s of cases) {
    const ref = enc.encode(s); // WHATWG ground-truth UTF-8 bytes
    const label = JSON.stringify(s).slice(0, 32);
    assert.strictEqual(utf8Bytes(s), ref.length, `oracle vs TextEncoder: ${label}`);
    assert.strictEqual(Buffer.byteLength(s, 'utf8'), ref.length, `byteLength: ${label}`);
    assert.deepStrictEqual(new Uint8Array(Buffer.from(s, 'utf8')), ref, `bytes: ${label}`);
  }
}
