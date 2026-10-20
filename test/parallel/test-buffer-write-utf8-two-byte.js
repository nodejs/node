'use strict';
// UTF-8 encoding of two-byte (UTF-16) JS strings through the Buffer write
// paths (Buffer.from, Buffer#write, Buffer.byteLength) must:
//  - produce standard UTF-8 for well-formed input of any size,
//  - replace lone surrogates with U+FFFD (EF BF BD),
//  - never write a partial character when the target is too small,
// independent of which internal fast path handles the string.
require('../common');
const assert = require('assert');

// Reference encoder written out longhand so the test does not depend on the
// implementation under test (TextEncoder shares code with it).
function utf8Reference(str) {
  const out = [];
  for (let i = 0; i < str.length; i++) {
    let cp = str.charCodeAt(i);
    if (cp >= 0xd800 && cp <= 0xdbff) {
      const next = i + 1 < str.length ? str.charCodeAt(i + 1) : 0;
      if (next >= 0xdc00 && next <= 0xdfff) {
        cp = 0x10000 + ((cp - 0xd800) << 10) + (next - 0xdc00);
        i++;
      } else {
        cp = 0xfffd;
      }
    } else if (cp >= 0xdc00 && cp <= 0xdfff) {
      cp = 0xfffd;
    }
    if (cp < 0x80) {
      out.push(cp);
    } else if (cp < 0x800) {
      out.push(0xc0 | (cp >> 6), 0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
      out.push(0xe0 | (cp >> 12), 0x80 | ((cp >> 6) & 0x3f), 0x80 | (cp & 0x3f));
    } else {
      out.push(0xf0 | (cp >> 18), 0x80 | ((cp >> 12) & 0x3f),
               0x80 | ((cp >> 6) & 0x3f), 0x80 | (cp & 0x3f));
    }
  }
  return Buffer.from(out);
}

function checkFull(str, label) {
  const expected = utf8Reference(str);
  assert.deepStrictEqual(Buffer.from(str, 'utf8'), expected, `${label}: Buffer.from`);
  assert.strictEqual(Buffer.byteLength(str, 'utf8'), expected.length, `${label}: byteLength`);
  // Exact-size target.
  const exact = Buffer.alloc(expected.length);
  assert.strictEqual(exact.write(str, 'utf8'), expected.length, `${label}: write exact`);
  assert.deepStrictEqual(exact, expected, `${label}: write exact bytes`);
  // Oversized target (3 bytes per code unit is what most internal callers allocate).
  const big = Buffer.alloc(str.length * 3 + 7, 0xaa);
  assert.strictEqual(big.write(str, 2, 'utf8'), expected.length, `${label}: write big`);
  assert.deepStrictEqual(big.subarray(2, 2 + expected.length), expected, `${label}: write big bytes`);
  assert.strictEqual(big[0], 0xaa);
  assert.strictEqual(big[2 + expected.length], 0xaa, `${label}: no overrun`);
}

// Truncating writes must stop before the first character that does not fit.
function checkTruncation(str, label) {
  const expected = utf8Reference(str);
  for (let size = 0; size <= Math.min(expected.length, 70); size++) {
    const target = Buffer.alloc(size + 1, 0xaa);
    const n = target.write(str, 0, size, 'utf8');
    assert.ok(n <= size, `${label}: size=${size} wrote ${n}`);
    assert.deepStrictEqual(target.subarray(0, n), expected.subarray(0, n), `${label}: prefix size=${size}`);
    assert.strictEqual(target[size], 0xaa, `${label}: overrun size=${size}`);
    // What was written must be a whole number of characters: the next byte in
    // the reference (if any) has to be a lead byte, not a continuation byte.
    if (n < expected.length) {
      assert.notStrictEqual(expected[n] & 0xc0, 0x80, `${label}: split char at size=${size}`);
      // And it stopped only because the next character really did not fit.
      let next = n + 1;
      while (next < expected.length && (expected[next] & 0xc0) === 0x80) next++;
      assert.ok(next > size, `${label}: stopped early at size=${size} (n=${n}, next=${next})`);
    }
  }
}

// Force a two-byte representation even for ASCII/Latin-1 content by building
// the string from a two-byte seed and slicing (V8 keeps the representation).
function twoByte(str) {
  const s = ('\u{1F600}' + str).slice(2);
  assert.strictEqual(s, str);
  return s;
}

const samples = {
  ascii: 'The quick brown fox jumps over the lazy dog 0123456789',
  latin1: 'français élan über naïve façade ÿ',
  bmp: '日本語テキストとハングル한국어',
  astral: 'emoji \u{1F600}\u{1F4A9} math \u{1D49C} han \u{20BB7}',
  mixed: 'a é 日 \u{1F600} b ü 本 \u{1F4A9}',
};

for (const [name, base] of Object.entries(samples)) {
  for (const repeat of [1, 3, 40, 700, 12000]) {
    const str = twoByte(base.repeat(repeat));
    checkFull(str, `${name} x${repeat}`);
  }
  checkTruncation(twoByte(base.repeat(3)), `${name} truncation`);
}

// Lone surrogates in various positions and sizes -> U+FFFD, rest intact.
const high = '\ud83d';
const low = '\ude00';
const surrogateCases = {
  'lone high': `ab${high}cd`,
  'lone low': `ab${low}cd`,
  'reversed pair': `ab${low}${high}cd`,
  'high at end': `abcd${high}`,
  'low at start': `${low}abcd`,
  'high high low': `${high}${high}${low}x`,
  'pair then lone': `${high}${low}${high}`,
  'only lone': high,
};
for (const [name, base] of Object.entries(surrogateCases)) {
  for (const pad of ['', 'x'.repeat(50), 'é'.repeat(300), '日'.repeat(30000)]) {
    const str = pad + base + pad;
    checkFull(str, `${name} pad=${pad.length}`);
  }
  checkTruncation(base + 'zz', `${name} truncation`);
}

// Buffer.from of a large two-byte string equals TextEncoder output.
{
  const str = twoByte(samples.mixed.repeat(50000));
  assert.deepStrictEqual(Buffer.from(str), Buffer.from(new TextEncoder().encode(str)));
}
