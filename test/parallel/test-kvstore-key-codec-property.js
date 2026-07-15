// Property-based tests for the order-preserving key codec. Unlike
// test-kvstore-key-codec.js's example-based regression tests, these generate
// many random composite keys per run and check invariants that must hold for
// *any* valid key — round-tripping, cross-type/same-type ordering, and prefix
// containment — using a comparator written independently from the
// implementation, so a codec bug cannot be silently mirrored in the check.
'use strict';

const { skipIfKVStoreMissing } = require('../common');
skipIfKVStoreMissing();

const assert = require('node:assert/strict');
const { describe, it } = require('node:test');
const { deserializeKeys, prefixBounds, serializeKeys } = require('internal/kvstore/keys');

const ITERATIONS = 3000;

// A pool deliberately mixing ASCII, Latin-1 supplement, CJK, and astral-plane
// (surrogate-pair) characters — UTF-16 code-unit order and UTF-8 byte order
// disagree exactly in these cases, which is the property worth fuzzing
// (strings sort by UTF-8 byte order per the key-codec spec).
const CHAR_POOL = [
  ...'abcXYZ019 _-',
  ...'éñüçÉÑ',
  ...'中文字符',
  ...'😀🎉🚀💥',
];

function randomChar() {
  return CHAR_POOL[Math.floor(Math.random() * CHAR_POOL.length)];
}

function randomString() {
  const length = 1 + Math.floor(Math.random() * 6);
  let out = '';
  for (let i = 0; i < length; i++) out += randomChar();
  return out || 'x';
}

function randomNumber() {
  const roll = Math.random();
  if (roll < 0.02) return Infinity;
  if (roll < 0.04) return -Infinity;
  if (roll < 0.06) return 0;
  if (roll < 0.08) return -0;
  const magnitude = Math.random() * 10 ** (Math.floor(Math.random() * 12) - 3);
  return Math.random() < 0.5 ? magnitude : -magnitude;
}

function randomBigint() {
  const digits = 1 + Math.floor(Math.random() * 30); // well under the 255-byte cap
  let hex = '';
  for (let i = 0; i < digits; i++) hex += Math.floor(Math.random() * 16).toString(16);
  const magnitude = BigInt(`0x${hex}`);
  return Math.random() < 0.5 ? magnitude : -magnitude;
}

function randomSegment() {
  switch (Math.floor(Math.random() * 4)) {
    case 0: return randomString();
    case 1: return randomNumber();
    case 2: return randomBigint();
    default: return Math.random() < 0.5;
  }
}

function randomKey(minLength = 1, maxLength = 4) {
  const length = minLength + Math.floor(Math.random() * (maxLength - minLength + 1));
  return Array.from({ length }, randomSegment);
}

function describeKey(key) {
  return `[${key.map((s) => (typeof s === 'bigint' ? `${s}n` : JSON.stringify(s))).join(', ')}]`;
}

// Independent reference comparator (documented ordering rules, not the
// codec's own encode/decode logic — a bug in the codec can't hide here).
const TYPE_ORDER = { string: 0, number: 1, bigint: 2, boolean: 3 };

function referenceCompareSegments(a, b) {
  const ta = typeof a;
  const tb = typeof b;
  if (ta !== tb) return TYPE_ORDER[ta] - TYPE_ORDER[tb];
  if (ta === 'string')
    return Buffer.compare(Buffer.from(a, 'utf8'), Buffer.from(b, 'utf8'));
  if (ta === 'number') {
    const na = Object.is(a, -0) ? 0 : a;
    const nb = Object.is(b, -0) ? 0 : b;
    return na < nb ? -1 : na > nb ? 1 : 0;
  }
  if (ta === 'bigint') return a < b ? -1 : a > b ? 1 : 0;
  return (a ? 1 : 0) - (b ? 1 : 0); // boolean: false < true
}

function referenceCompareKeys(keyA, keyB) {
  const len = Math.min(keyA.length, keyB.length);
  for (let i = 0; i < len; i++) {
    const c = referenceCompareSegments(keyA[i], keyB[i]);
    if (c !== 0) return c < 0 ? -1 : 1;
  }
  return Math.sign(keyA.length - keyB.length);
}

function normalizeForRoundTrip(key) {
  // The only lossy, documented normalization: -0 collapses to 0.
  return key.map((s) => (typeof s === 'number' && Object.is(s, -0) ? 0 : s));
}

describe('key codec property tests (fuzz)', () => {
  it(`round-trip: deserializeKeys(serializeKeys(key)) === key, for ${ITERATIONS} random keys`, () => {
    for (let i = 0; i < ITERATIONS; i++) {
      const key = randomKey();
      const encoded = serializeKeys(key);
      const decoded = deserializeKeys(encoded);
      assert.deepEqual(
        decoded,
        normalizeForRoundTrip(key),
        `round-trip mismatch for key ${describeKey(key)}`,
      );
    }
  });

  it(`order-preserving: encoded byte-string order matches the documented composite-key order, for ${ITERATIONS} random pairs`, () => {
    for (let i = 0; i < ITERATIONS; i++) {
      const keyA = randomKey();
      const keyB = randomKey();
      const expected = referenceCompareKeys(keyA, keyB);

      const encodedA = serializeKeys(keyA);
      const encodedB = serializeKeys(keyB);
      const actual = encodedA < encodedB ? -1 : encodedA > encodedB ? 1 : 0;

      assert.strictEqual(
        actual,
        expected,
        `order mismatch: expected ${describeKey(keyA)} ${expected < 0 ? '<' : expected > 0 ? '>' : '=='} ${describeKey(keyB)}, but encoded forms disagreed`,
      );
    }
  });

  it(`prefix containment: an extended key always falls within its prefix's bounds; the prefix key itself never does, for ${ITERATIONS} random cases`, () => {
    for (let i = 0; i < ITERATIONS; i++) {
      const prefix = randomKey(1, 3);
      const extension = randomKey(1, 2);
      const extended = [...prefix, ...extension];

      const encodedPrefix = serializeKeys(prefix);
      const encodedExtended = serializeKeys(extended);
      const bounds = prefixBounds(encodedPrefix);

      assert.ok(
        encodedExtended >= bounds.start,
        `extended key ${describeKey(extended)} should be >= prefix ${describeKey(prefix)}'s start bound`,
      );
      assert.ok(
        bounds.end === null || encodedExtended < bounds.end,
        `extended key ${describeKey(extended)} should be < prefix ${describeKey(prefix)}'s end bound`,
      );
      assert.ok(
        encodedPrefix < bounds.start,
        `the exact prefix key ${describeKey(prefix)} must sort before its own start bound (never self-matches)`,
      );
    }
  });
});
