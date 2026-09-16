'use strict';

require('../common');
const assert = require('assert');
const { Buffer } = require('buffer');

const encodings = ['utf8', 'utf-8', 'UTF8', 'ucs2', 'utf16le', 'UTF-16LE',
                   'latin1', 'binary', 'ascii', 'hex', 'base64', 'base64url'];

function check(buf, encoding = 'utf8') {
  const expected = buf.toString(encoding).length;
  assert.strictEqual(Buffer.stringLength(buf, encoding), expected,
                     `${encoding}: ${buf.toString('hex').slice(0, 64)}`);
}

// Fixed-width encodings on a range of lengths, including odd ones.
for (let len = 0; len <= 20; len++) {
  const buf = Buffer.alloc(len, 0xE9);
  for (const encoding of encodings) check(buf, encoding);
}

// Default encoding is utf8.
assert.strictEqual(Buffer.stringLength(Buffer.from('€ 100')), 5);
assert.strictEqual(Buffer.stringLength(Buffer.from('😀')), 2);
assert.strictEqual(Buffer.stringLength(Buffer.alloc(0)), 0);

// Every kind of input that toString-like APIs accept.
{
  const source = Buffer.from('héllo wörld €');
  const ab = new ArrayBuffer(source.length);
  new Uint8Array(ab).set(source);
  const sab = new SharedArrayBuffer(source.length);
  new Uint8Array(sab).set(source);
  const expected = source.toString().length;
  assert.strictEqual(Buffer.stringLength(ab), expected);
  assert.strictEqual(Buffer.stringLength(sab), expected);
  assert.strictEqual(Buffer.stringLength(new Uint8Array(ab)), expected);
  assert.strictEqual(Buffer.stringLength(new Uint16Array(ab.slice(0, 16))),
                     Buffer.from(ab.slice(0, 16)).toString().length);
}

// Detached ArrayBuffers are empty.
{
  const ab = new ArrayBuffer(8);
  const view = new Uint8Array(ab);
  structuredClone(ab, { transfer: [ab] });
  assert.strictEqual(Buffer.stringLength(ab), 0);
  assert.strictEqual(Buffer.stringLength(view), 0);
}

// Invalid UTF-8 is counted the way toString() decodes it (one U+FFFD per
// maximal subpart). Cases taken from test/fixtures/wpt/encoding.
[
  [0xFF],
  [0xC0],
  [0xE0],
  [0xC0, 0x00],
  [0xC0, 0xC0],
  [0xE0, 0x00],
  [0xE0, 0xC0],
  [0xE0, 0x80, 0x00],
  [0xE0, 0x80, 0xC0],
  [0xFC, 0x80, 0x80, 0x80, 0x80, 0x80],
  [0xFE, 0x80, 0x80, 0x80, 0x80, 0x80],
  [0xC0, 0x80],
  [0xE0, 0x80, 0x80],
  [0xF0, 0x80, 0x80, 0x80],
  [0xF8, 0x80, 0x80, 0x80, 0x80],
  [0xFC, 0x80, 0x80, 0x80, 0x80, 0x80],
  [0xC1, 0xBF],
  [0xE0, 0x81, 0xBF],
  [0xF0, 0x80, 0x81, 0xBF],
  [0xF8, 0x80, 0x80, 0x81, 0xBF],
  [0xFC, 0x80, 0x80, 0x80, 0x81, 0xBF],
  [0xE0, 0x9F, 0xBF],
  [0xF0, 0x8F, 0xBF, 0xBF],
  [0xF8, 0x87, 0xBF, 0xBF, 0xBF],
  [0xFC, 0x83, 0xBF, 0xBF, 0xBF, 0xBF],
  [0xED, 0xA0, 0x80],
  [0xED, 0xBF, 0xBF],
  [0xF4, 0x90, 0x80, 0x80],
  [0xF0, 0x9F, 0x98],
  [0xE2, 0x82],
  [0xE2, 0x82, 0x41],
  [0xF0, 0x9F, 0x98, 0x80, 0xF0, 0x9F],
  [0x41, 0xF0, 0x9F, 0x98, 0x41],
].forEach((bytes) => {
  const invalid = Buffer.from(bytes);
  check(invalid);
  check(Buffer.concat([Buffer.from('abc'), invalid, Buffer.from('€😀')]));
  // Push the invalid sequence past the SIMD fast path's prefix.
  check(Buffer.concat([Buffer.alloc(100, 'é'), invalid, Buffer.alloc(100, 'x')]));
});

// Random byte soup, checked against the real decoder.
{
  let seed = 0x2545F491;
  const next = () => (seed = (seed * 1103515245 + 12345) >>> 0);
  for (let i = 0; i < 2000; i++) {
    const len = next() % 64;
    const buf = Buffer.alloc(len);
    for (let j = 0; j < len; j++) {
      // Bias towards UTF-8 structural bytes so multi-byte prefixes happen.
      const r = next() % 8;
      buf[j] = r < 3 ? 0x80 + (next() % 0x40) :
        r < 5 ? 0xC0 + (next() % 0x40) : next() % 0x100;
    }
    check(buf);
  }
}

// Large inputs (above the 1 MiB single-pass threshold of the decoder).
{
  const big = Buffer.alloc(3 * 2 ** 20, '€');
  check(big);
  check(Buffer.concat([big, Buffer.from([0xE2, 0x82])]));
  check(Buffer.alloc(2 ** 20 + 1, 'a'), 'base64');
  check(Buffer.alloc(2 ** 20 + 1, 'a'), 'base64url');
}

// Argument validation.
for (const input of [undefined, null, 'abc', 42, {}, [], new Blob([]),
                     new DataView(new ArrayBuffer(4))]) {
  assert.throws(() => Buffer.stringLength(input),
                { code: 'ERR_INVALID_ARG_TYPE' });
}
for (const encoding of ['nope', 'utf32', '']) {
  assert.throws(() => Buffer.stringLength(Buffer.alloc(1), encoding),
                { code: 'ERR_UNKNOWN_ENCODING' });
}
for (const encoding of [1, null, {}]) {
  assert.throws(() => Buffer.stringLength(Buffer.alloc(1), encoding),
                { code: 'ERR_INVALID_ARG_TYPE' });
}
