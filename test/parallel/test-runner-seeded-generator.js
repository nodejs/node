// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const { createSeededGenerator, kMaxRandomSeed } = require('internal/test_runner/utils');

function sequence(seed, length = 10) {
  const random = createSeededGenerator(seed);
  const values = [];
  for (let i = 0; i < length; i++) {
    values.push(random());
  }
  return values;
}

// The same seed must always produce the same sequence so that
// --test-random-seed reproduces a randomized test order across runs.
assert.deepStrictEqual(sequence(0), sequence(0));
assert.deepStrictEqual(sequence(12345), sequence(12345));
assert.deepStrictEqual(sequence(kMaxRandomSeed), sequence(kMaxRandomSeed));

// Different seeds must produce different sequences.
assert.notDeepStrictEqual(sequence(11111), sequence(22222));
assert.notDeepStrictEqual(sequence(0), sequence(1));

// Seeds are coerced to uint32, matching the range accepted by
// --test-random-seed.
assert.deepStrictEqual(sequence(1), sequence(1 + 2 ** 32));

// All generated values must fall in [0, 1).
for (const value of sequence(98765, 1000)) {
  assert.ok(value >= 0 && value < 1, `value ${value} out of range`);
}
