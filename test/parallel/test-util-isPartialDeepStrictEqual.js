'use strict';

// Confirm functionality of `util.isPartialDeepStrictEqual()`.

require('../common');

const assert = require('assert');
const util = require('util');
const { test } = require('node:test');

test('util.isPartialDeepStrictEqual returns true with expected properties', () => {
  assert.strictEqual(
    util.isPartialDeepStrictEqual({ a: 1, b: 2, c: 3 }, { b: 2 }),
    true,
  );

  assert.strictEqual(
    util.isPartialDeepStrictEqual({ a: { b: { c: 1 } } }, { a: { b: { c: 1 } } }),
    true,
  );

  assert.strictEqual(
    util.isPartialDeepStrictEqual([1, 2, 3, 4, 5, 6, 7, 8, 9], [4, 5, 8]),
    true,
  );

  {
    const obj = { a: 1 };
    assert.strictEqual(
      util.isPartialDeepStrictEqual(obj, obj),
      true,
    );
  }

  assert.strictEqual(
    util.isPartialDeepStrictEqual({ a: 1, b: 2 }, { b: 2, a: 1 }),
    true,
  );
});

test('util.isPartialDeepStrictEqual returns false with unexpected properties', () => {
  assert.strictEqual(
    util.isPartialDeepStrictEqual({ a: 1 }, { a: 2 }),
    false,
  );

  assert.strictEqual(
    util.isPartialDeepStrictEqual({ a: 1 }, { b: 1 }),
    false,
  );

  assert.strictEqual(
    util.isPartialDeepStrictEqual([1, 'two'], [1, 'two', true]),
    false,
  );
});
