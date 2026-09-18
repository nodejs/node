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
