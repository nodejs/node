'use strict';

// Tests validation of the CBOR payload accepted by importHistogram().

require('../common');
const assert = require('node:assert');
const { createHistogram, importHistogram } = require('node:perf_hooks');

// Minimal CBOR (RFC 8949) encoding helpers for building payloads. Each
// helper returns an array of bytes.
function head(major, value) {
  const v = BigInt(value);
  const m = major << 5;
  if (v < 24n) return [m | Number(v)];
  const bytes = [];
  const width = v < 0x100n ? 1 : v < 0x10000n ? 2 : v < 0x100000000n ? 4 : 8;
  for (let i = width - 1; i >= 0; i--) {
    bytes.push(Number((v >> BigInt(i * 8)) & 0xffn));
  }
  return [m | { 1: 24, 2: 25, 4: 26, 8: 27 }[width], ...bytes];
}
const uint = (value) => head(0, value);
const negint = (value) => head(1, value);  // Encodes -1 - value.
const text = (str) => [...head(3, Buffer.byteLength(str)), ...Buffer.from(str)];
function f64(value) {
  const buf = Buffer.alloc(9);
  buf[0] = 0xfb;
  buf.writeDoubleBE(value, 1);
  return [...buf];
}

function array(items) {
  const out = head(4, items.length);
  for (const item of items) out.push(...item);
  return out;
}

function map(entries) {
  const out = head(5, entries.length);
  for (const { 0: key, 1: value } of entries) out.push(...key, ...value);
  return out;
}
const importBytes = (bytes) => importHistogram(new Uint8Array(bytes));
const kInvalid = { code: 'ERR_INVALID_ARG_VALUE' };

// lowest=1 (the default), highest=100, figures=1 produces counts_len=64, with
// indexes below 32 mapping to the identical values.
const kLayout = [
  [uint(2), uint(100)],  // highest
  [uint(3), uint(1)],    // figures
  [uint(9), uint(64)],   // counts length
];
const counts = (...pairs) => [uint(10), array(pairs.map((v) => uint(v)))];

{
  // Absent total count, min, and max are derived from the counts.
  const h = importBytes(map([...kLayout, counts(5, 2, 3, 1)]));
  assert.strictEqual(h.count, 3);
  assert.strictEqual(h.min, 5);
  assert.strictEqual(h.max, 8);
  assert.strictEqual(h.percentile(50), 5);
  assert.strictEqual(h.percentile(100), 8);
}

{
  // A total count that is present must match the counts.
  const h = importBytes(map([...kLayout, [uint(4), uint(3)], counts(5, 2, 3, 1)]));
  assert.strictEqual(h.count, 3);
  assert.throws(
    () => importBytes(map([...kLayout, [uint(4), uint(4)], counts(5, 2, 3, 1)])),
    kInvalid);
  assert.throws(
    () => importBytes(map([...kLayout, [uint(4), uint(1)]])),
    kInvalid);
}

{
  // Min and max values that are present are restored as recorded.
  const h = createHistogram();
  h.record(987654321);
  h.record(1234567891);
  const h2 = importHistogram(h.export());
  assert.strictEqual(h2.min, h.min);
  assert.strictEqual(h2.max, h.max);
}

// Duplicate keys are rejected.
assert.throws(() => importBytes(map([...kLayout, [uint(3), uint(1)]])),
              kInvalid);
assert.throws(
  () => importBytes(map([...kLayout, counts(5, 2), counts(6, 7)])),
  kInvalid);
assert.throws(() => importBytes(map([
  ...kLayout,
  [uint(11), map([[uint(0), f64(0.5)], [uint(0), f64(0.5)]])],
])), kInvalid);

// Keys must be unsigned integers.
assert.throws(() => importBytes(map([...kLayout, [text('a'), uint(1)]])),
              kInvalid);
assert.throws(() => importBytes(map([...kLayout, [negint(0), uint(1)]])),
              kInvalid);

// Values must have the expected CBOR types.
assert.throws(() => importBytes(map([
  [uint(2), uint(100)], [uint(3), text('1')], [uint(9), uint(64)],
])), kInvalid);
assert.throws(() => importBytes(map([
  [uint(2), negint(99)], [uint(3), uint(1)], [uint(9), uint(64)],
])), kInvalid);
assert.throws(() => importBytes(map([...kLayout, [uint(10), map([])]])),
              kInvalid);
assert.throws(() => importBytes(map([...kLayout, [uint(11), array([])]])),
              kInvalid);

// Values must not be truncated when casting to the field's type.
assert.throws(() => importBytes(map([
  [uint(2), uint(100)], [uint(3), uint(1)], [uint(9), uint(2n ** 32n + 64n)],
])), kInvalid);
assert.throws(() => importBytes(map([
  [uint(2), uint(100)], [uint(3), uint(2n ** 32n + 1n)], [uint(9), uint(64)],
])), kInvalid);
assert.throws(() => importBytes(map([...kLayout, counts(5, 2n ** 63n)])),
              kInvalid);
assert.throws(() => importBytes(map([...kLayout, counts(2n ** 31n, 1)])),
              kInvalid);

// Counts must not overflow when added up.
assert.throws(() => importBytes(map([
  ...kLayout,
  counts(1, 2n ** 62n, 1, 2n ** 62n, 1, 2n ** 62n),
])), kInvalid);

{
  // Sparse count indexes must be strictly increasing. Only the first delta,
  // which is an absolute index, may be zero.
  const h = importBytes(map([...kLayout, counts(0, 1, 5, 1)]));
  assert.strictEqual(h.count, 2);
  assert.throws(() => importBytes(map([...kLayout, counts(5, 2, 0, 1)])),
                kInvalid);
}

// --- Format versions ---

function assertSameHistogram(actual, expected) {
  assert.strictEqual(actual.count, expected.count);
  assert.strictEqual(actual.min, expected.min);
  assert.strictEqual(actual.max, expected.max);
  assert.strictEqual(actual.percentile(50), expected.percentile(50));
  assert.strictEqual(actual.percentile(100), expected.percentile(100));
}

{
  // Version 1 data, as produced by Node.js v26.9.0, remains importable.
  const v1 = Buffer.from(
    'ac00010101021b001fffffffffffff030304070501061a075bcd15070008fb3f' +
    'f00000000000000919b0000a8c010101010101190bfd02191fa101191bba010b' +
    'a500fb3fc45d819a94b14c01fb4172dc620791dc2002fb431ce79e5650942003' +
    'fb3fd2bec33301886804191388', 'hex');
  const h = importHistogram(new Uint8Array(v1));
  assert.strictEqual(h.count, 7);
  assert.strictEqual(h.min, 1);
  assert.strictEqual(h.max, 123469823);
  assert.strictEqual(h.mean, 17777921.42857143);
  assert.strictEqual(h.stddev, 43136537.01467338);
  assert.strictEqual(h.percentile(50), 4099);
  assert.strictEqual(h.percentile(100), 123469823);
  assert.strictEqual(h.ewmaMean, 19777056.47311032);
  assert.strictEqual(h.ewmaStddev, 45099796.52633914);
  assert.strictEqual(h.ewmaErrorRate, 0.29289321881345254);
}

{
  // export() produces version 2 data, which round-trips.
  const h = createHistogram({ halfLife: 4, threshold: 5000 });
  for (const value of [1, 2, 3, 4096, 4097, 1000000, 123456789]) {
    h.record(value);
  }
  const data = h.export();
  // The first map entry is the version.
  assert.deepStrictEqual([...data.subarray(1, 3)], [...uint(0), ...uint(2)]);
  const h2 = importHistogram(data);
  assertSameHistogram(h2, h);
  assert.strictEqual(h2.ewmaMean, h.ewmaMean);
  assert.strictEqual(h2.ewmaErrorRate, h.ewmaErrorRate);
}

{
  // Unknown keys in version 2 data are ignored, whatever their values are.
  const unknownValues = [
    uint(2n ** 40n),
    negint(7),
    [...head(2, 3), 1, 2, 3],                      // Byte string.
    text('hello'),
    array([uint(1), array([text('nested')])]),
    map([[text('a'), uint(1)], [uint(99), map([[uint(1), f64(2.5)]])]]),
    [...head(6, 1), ...text('2026-09-17')],        // Tag.
    f64(1.5),
    [0xf9, 0x3e, 0x00],                            // Float16.
    [0xfa, 0x3f, 0xc0, 0x00, 0x00],                // Float32.
    [0xf5],                                        // true
    [0xf6],                                        // null
    [0xf8, 0xff],                                  // Simple value 255.
  ];
  const expected = importBytes(
    map([[uint(0), uint(2)], ...kLayout, counts(5, 2, 3, 1)]));
  const h = importBytes(map([
    [uint(0), uint(2)],
    ...kLayout,
    ...unknownValues.map((value, n) => [uint(12 + n), value]),
    counts(5, 2, 3, 1),
  ]));
  assertSameHistogram(h, expected);

  // Unknown keys may appear before the version key.
  assertSameHistogram(importBytes(map([
    [uint(12), text('before the version')],
    [uint(0), uint(2)],
    ...kLayout,
    counts(5, 2, 3, 1),
  ])), expected);

  // Unknown keys in the EWMA state are ignored as well.
  const withEwma = importBytes(map([
    [uint(0), uint(2)],
    ...kLayout,
    counts(5, 2, 3, 1),
    [uint(11), map([
      [uint(0), f64(0.5)],
      [uint(99), text('unknown')],
      [uint(1), f64(6)],
    ])],
  ]));
  assertSameHistogram(withEwma, expected);
  assert.strictEqual(withEwma.ewmaMean, 6);
}

{
  // Version 1 data, or data without a version, keeps the original semantics:
  // unknown keys are rejected.
  const unknown = [uint(12), uint(0)];
  assert.throws(() => importBytes(map([[uint(0), uint(1)], ...kLayout, unknown])),
                kInvalid);
  assert.throws(() => importBytes(map([unknown, [uint(0), uint(1)], ...kLayout])),
                kInvalid);
  assert.throws(() => importBytes(map([...kLayout, unknown])), kInvalid);
  assert.throws(() => importBytes(map([
    [uint(0), uint(1)],
    ...kLayout,
    [uint(11), map([[uint(99), uint(0)]])],
  ])), kInvalid);
}

// Other versions are rejected.
for (const version of [0, 3, 99]) {
  assert.throws(
    () => importBytes(map([[uint(0), uint(version)], ...kLayout])),
    kInvalid);
}

{
  // Values of unknown keys must still be well-formed.
  const withUnknownValue =
    (value) => map([[uint(0), uint(2)], ...kLayout, [uint(12), value]]);

  // Indefinite-length items are not supported.
  assert.throws(() => importBytes(withUnknownValue([0x5f, 0x41, 0x00, 0xff])),
                kInvalid);
  assert.throws(() => importBytes(withUnknownValue([0x9f, 0x00, 0xff])),
                kInvalid);
  // Reserved additional information and break codes.
  assert.throws(() => importBytes(withUnknownValue([0x1c])), kInvalid);
  assert.throws(() => importBytes(withUnknownValue([0xfc])), kInvalid);
  assert.throws(() => importBytes(withUnknownValue([0xff])), kInvalid);
  // Truncated values.
  assert.throws(() => importBytes(withUnknownValue([...head(3, 10), 0x61])),
                kInvalid);
  assert.throws(() => importBytes(withUnknownValue([0xfb, 0x00, 0x00])),
                kInvalid);
  assert.throws(() => importBytes(withUnknownValue(head(4, 5))), kInvalid);
  // Nesting is limited to 16 levels.
  const nested = (depth) => [...new Array(depth).fill(0x81), 0x00];
  assert.strictEqual(importBytes(withUnknownValue(nested(16))).count, 0);
  assert.throws(() => importBytes(withUnknownValue(nested(17))), kInvalid);
}
