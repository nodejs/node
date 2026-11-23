// Flags: --expose-internals
'use strict';

// Checks the protobuf encoding of the pprof messages.

const common = require('../common');
const assert = require('node:assert');
const { decodeProfile } = require('../common/pprof');
const {
  encodeNumber,
  measureNumber,
  varintLength,
} = require('internal/perf/pprof/protobuf');
const {
  Label,
  Line,
  Location,
  Mapping,
  Profile,
  ProfileFunction,
  Sample,
  StringTable,
  ValueType,
} = require('internal/perf/pprof/profile');

/**
 * Formats bytes as a hexadecimal string.
 * @param {Buffer | Uint8Array | number[]} bytes The bytes to format.
 * @returns {string} Two hexadecimal digits per byte, without separators.
 */
function hex(bytes) {
  return Buffer.from(bytes).toString('hex');
}

// Varints.
for (const { number, encoded } of [
  { number: 0, encoded: '00' },
  { number: 1, encoded: '01' },
  { number: 127, encoded: '7f' },
  { number: 128, encoded: '8001' },
  { number: 300, encoded: 'ac02' },
  { number: 16384, encoded: '808001' },
  { number: 2 ** 32 - 1, encoded: 'ffffffff0f' },
  { number: 2 ** 32, encoded: '8080808010' },
  { number: Number.MAX_SAFE_INTEGER, encoded: 'ffffffffffffff0f' },
  { number: 2n ** 63n, encoded: '80808080808080808001' },
  { number: 2n ** 64n - 1n, encoded: 'ffffffffffffffffff01' },
  // Negative numbers are encoded as 64-bit two's complement.
  { number: -1, encoded: 'ffffffffffffffffff01' },
  { number: -1n, encoded: 'ffffffffffffffffff01' },
  { number: -2, encoded: 'feffffffffffffffff01' },
]) {
  const buffer = new Uint8Array(10);
  const end = encodeNumber(buffer, 0, number);
  assert.strictEqual(hex(buffer.subarray(0, end)), encoded, `${number}`);
  assert.strictEqual(varintLength(number), encoded.length / 2, `${number}`);
  const isZero = number === 0;
  assert.strictEqual(measureNumber(number), isZero ? 0 : encoded.length / 2);
}

// Default values are omitted.
for (const message of [
  new ValueType({}),
  new Label({}),
  new Line({}),
  new Mapping({}),
  new ProfileFunction({}),
  new Location({}),
  new Sample({}),
]) {
  assert.strictEqual(message.encode().length, 0);
}

assert.strictEqual(hex(new ValueType({ type: 1, unit: 2 }).encode()),
                   '08011002');
assert.strictEqual(hex(new Mapping({ id: 1, hasFunctions: true }).encode()),
                   '08013801');
// Packed repeated fields keep zeros, to preserve indices.
const locationId = [
  1,
  0,
  300,
];
assert.strictEqual(hex(new Sample({ locationId }).encode()), '0a040100ac02');
// Sub-messages are length-delimited.
assert.strictEqual(
  hex(new Location({ id: 1, line: [{ functionId: 2, line: 3 }] }).encode()),
  '0801220408021003');

// The string table starts with the empty string, and deduplicates strings.
{
  const table = new StringTable();
  assert.strictEqual(table.dedup(''), 0);
  assert.strictEqual(table.dedup('a'), 1);
  assert.strictEqual(table.dedup('€'), 2);
  assert.strictEqual(table.dedup('a'), 1);
  assert.deepStrictEqual(table.strings, [
    '',
    'a',
    '€',
  ]);
  assert.strictEqual(hex(new Profile({ stringTable: table }).encode()),
                     '32003201613203e282ac');
}

// A whole profile round-trips through an independent decoder.
{
  const stringTable = new StringTable();
  const wall = {
    type: stringTable.dedup('wall'),
    unit: stringTable.dedup('nanoseconds'),
  };
  const profile = new Profile({
    sampleType: [
      { type: stringTable.dedup('sample'), unit: stringTable.dedup('count') },
      wall,
    ],
    sample: [
      { locationId: [2, 1], value: [3, 3000000] },
      { locationId: [1], value: [0, 0] },
    ],
    location: [
      { id: 1, line: [{ functionId: 1, line: 10 }] },
      { id: 2, line: [{ functionId: 2, line: 20 }] },
    ],
    function: [
      {
        id: 1,
        name: stringTable.dedup('outer'),
        filename: stringTable.dedup('a.js'),
      },
      {
        id: 2,
        name: stringTable.dedup('inner'),
        filename: stringTable.dedup('a.js'),
      },
    ],
    stringTable,
    timeNanos: 1700000000000000000,
    durationNanos: 2 ** 40,
    periodType: wall,
    period: 1000000,
  });
  const encoded = profile.encode();
  assert.ok(Buffer.isBuffer(encoded));

  const decoded = decodeProfile(encoded);
  assert.deepStrictEqual(decoded.stringTable, stringTable.strings);
  assert.deepStrictEqual(decoded.sampleType, [
    { type: 3, unit: 4 },
    { type: 1, unit: 2 },
  ]);
  assert.deepStrictEqual(decoded.sample, [
    { locationId: [2, 1], value: [3, 3000000] },
    { locationId: [1], value: [0, 0] },
  ]);
  assert.deepStrictEqual(decoded.location.map((l) => l.line[0].line), [10, 20]);
  assert.deepStrictEqual(decoded.function.map((f) => f.name), [5, 7]);
  assert.strictEqual(BigInt(decoded.timeNanos), 1700000000000000000n);
  assert.strictEqual(decoded.durationNanos, 2 ** 40);
  assert.deepStrictEqual(decoded.periodType, { type: 1, unit: 2 });
  assert.strictEqual(decoded.period, 1000000);

  profile.encodeAsync().then(common.mustCall((asyncEncoded) => {
    assert.deepStrictEqual(asyncEncoded, encoded);
  }));
}
