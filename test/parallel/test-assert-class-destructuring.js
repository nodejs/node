'use strict';

require('../common');
const assert = require('assert');
const { Assert } = require('assert');
const { test } = require('node:test');

// Disable colored output to prevent color codes from breaking assertion
// message comparisons. This should only be an issue when process.stdout
// is a TTY.
if (process.stdout.isTTY) {
  process.env.NODE_DISABLE_COLORS = '1';
}

test('Assert class destructuring behavior - diff option', () => {
  const assertInstanceFull = new Assert({ diff: 'full' });
  const assertInstanceSimple = new Assert({ diff: 'simple' });

  assertInstanceFull.throws(
    () => assertInstanceFull.strictEqual({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.diff, 'full');
      return true;
    }
  );

  assertInstanceSimple.throws(
    () => assertInstanceSimple.strictEqual({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.diff, 'simple');
      return true;
    }
  );

  const { strictEqual: strictEqualSimple } = assertInstanceSimple;
  const { strictEqual: strictEqualFull } = assertInstanceFull;
  const { deepStrictEqual: deepStrictEqualFull } = assertInstanceFull;
  const { equal: equalFull } = assertInstanceFull;

  assert.throws(
    () => strictEqualSimple({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.diff, 'simple');
      return true;
    }
  );

  assert.throws(
    () => strictEqualFull({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.diff, 'simple');
      return true;
    }
  );

  assert.throws(
    () => deepStrictEqualFull({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.diff, 'simple');
      return true;
    }
  );

  assert.throws(
    () => equalFull({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.diff, 'simple');
      return true;
    }
  );
});

test('Assert class destructuring behavior - strict option', () => {
  const assertInstanceNonStrict = new Assert({ strict: false });
  const assertInstanceStrict = new Assert({ strict: true });

  assertInstanceNonStrict.equal(2, '2');

  assert.throws(
    () => assertInstanceStrict.equal(2, '2'),
    assert.AssertionError
  );

  const { equal: equalNonStrict } = assertInstanceNonStrict;
  const { equal: equalStrict } = assertInstanceStrict;

  equalNonStrict(2, '2');
  assert.throws(
    () => equalStrict(2, '2'),
    assert.AssertionError
  );
});

test('Assert class destructuring behavior - comprehensive methods', () => {
  const myAssert = new Assert({ diff: 'full', strict: false });

  const {
    fail,
    equal,
    strictEqual,
    deepStrictEqual,
    throws,
    match,
    doesNotMatch
  } = myAssert;

  assert.throws(() => fail('test message'), (err) => {
    assert.strictEqual(err.diff, 'simple');
    assert.strictEqual(err.message, 'test message');
    return true;
  });

  assert.throws(() => equal({ a: 1 }, { a: 2 }), (err) => {
    assert.strictEqual(err.diff, 'simple');
    return true;
  });

  assert.throws(() => strictEqual({ a: 1 }, { a: 2 }), (err) => {
    assert.strictEqual(err.diff, 'simple');
    return true;
  });

  assert.throws(() => deepStrictEqual({ a: 1 }, { a: 2 }), (err) => {
    assert.strictEqual(err.diff, 'simple');
    return true;
  });

  throws(() => { throw new Error('test'); }, Error);

  match('hello world', /world/);

  doesNotMatch('hello world', /xyz/);
});
