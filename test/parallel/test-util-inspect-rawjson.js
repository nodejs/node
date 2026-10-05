'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

const cases = [
  [null, 'RawJSON { null }'],
  ['null', 'RawJSON { null }'],
  [true, 'RawJSON { true }'],
  ['true', 'RawJSON { true }'],
  [false, 'RawJSON { false }'],
  ['false', 'RawJSON { false }'],
  ['"blep"', "RawJSON { 'blep' }"],
  ['""', "RawJSON { '' }"],
  [42, 'RawJSON { 42 }'],
  ['42', 'RawJSON { 42 }'],
  [-0, 'RawJSON { 0 }'],
  ['-0', 'RawJSON { -0 }'],
  [1.25, 'RawJSON { 1.25 }'],
  ['1.25', 'RawJSON { 1.25 }'],
  [42.00, 'RawJSON { 42 }'],
  ['42.00', 'RawJSON { 42.00 }'],
  [1e3, 'RawJSON { 1000 }'],
  ['1e3', 'RawJSON { 1e3 }'],
  [12345678901234567890n, 'RawJSON { 12345678901234567890 }'],
  ['12345678901234567890', 'RawJSON { 12345678901234567890 }'],
];

for (const [raw, expected] of cases) {
  const value = JSON.rawJSON(raw);
  assert.strictEqual(util.inspect(value), expected);
  assert.strictEqual(util.inspect(value, { showHidden: true }), expected);
}

// When raw number differs from normalized number representation,
// numeric formatting must be disabled
assert.strictEqual(
  util.inspect(JSON.rawJSON('42'), { numericSeparator: true }),
  'RawJSON { 42 }',
);
assert.strictEqual(
  util.inspect(JSON.rawJSON('-0'), { numericSeparator: true }),
  'RawJSON { -0 }',
);
assert.strictEqual(
  util.inspect(JSON.rawJSON('-0.00'), { numericSeparator: true }),
  'RawJSON { -0.00 }',
);
assert.strictEqual(
  util.inspect(JSON.rawJSON('1234567'), { numericSeparator: true }),
  'RawJSON { 1_234_567 }',
);
assert.strictEqual(
  util.inspect(JSON.rawJSON('1234567.00'), { numericSeparator: true }),
  'RawJSON { 1234567.00 }',
);
assert.strictEqual(
  util.inspect(JSON.rawJSON('1.234567e+6'), { numericSeparator: true }),
  'RawJSON { 1.234567e+6 }',
);
assert.strictEqual(
  util.inspect(JSON.rawJSON('12345678901234567890'), { numericSeparator: true }),
  'RawJSON { 12345678901234567890 }',
);

// Test for false positive duck typing
assert.strictEqual(
  util.inspect({ __proto__: null, rawJSON: 'null' }),
  "[Object: null prototype] { rawJSON: 'null' }",
);
