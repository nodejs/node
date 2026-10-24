'use strict';

// Regression test for https://github.com/nodejs/node/issues/50397
// `assert.deepStrictEqual()` fails whenever the prototypes of the two values
// differ, but the generated diff may not make that cause obvious (e.g. when
// both values are inspected identically). The error message must surface the
// mismatch of the top-level prototypes explicitly. Prototype differences of
// nested objects are out of scope of the diagnostic.

require('../common');
const assert = require('assert');

// Disable colored output to prevent color codes from breaking assertion
// message comparisons.
if (process.stdout.isTTY)
  process.env.NODE_DISABLE_COLORS = '1';

class ExtendedArray extends Array {}

// The issue's reproduction: the diff only shows the class name prefix of the
// inspected value, which is easy to miss. The message must state the
// prototype mismatch explicitly.
{
  const actual = new ExtendedArray('hello');
  assert.throws(
    () => assert.deepStrictEqual(actual, ['hello']),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(
        err.message,
        /Object prototypes differ: ExtendedArray !== Array/,
      );
      return true;
    },
  );
}

// Both values may even be inspected identically, in which case the structural
// diff alone gives no hint at all about the prototype difference.
{
  const actual = new (class {})();
  assert.strictEqual(
    require('util').inspect(actual),
    require('util').inspect({}),
  );
  assert.throws(
    () => assert.deepStrictEqual(actual, {}),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(
        err.message,
        /Object prototypes differ: \(anonymous\) !== Object/,
      );
      return true;
    },
  );
}

// The hint must not be added when the prototypes are identical.
{
  assert.throws(
    () => assert.deepStrictEqual({ a: 1 }, { a: 2 }),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ/);
      return true;
    },
  );
}

// Cross-kind comparisons between default prototypes (Array vs Object) render
// differently already, so no hint is needed.
{
  assert.throws(
    () => assert.deepStrictEqual([], {}),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ/);
      return true;
    },
  );
}

// Null-prototype objects are inspected with an explicit prefix, so they are
// not hidden either and no hint is added.
{
  assert.throws(
    () => assert.deepStrictEqual({ __proto__: null }, {}),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ/);
      return true;
    },
  );
}

// Operators that do not require prototype equality must not gain the hint.
{
  const actual = new ExtendedArray('hello');
  // Loose deep equality ignores prototypes entirely, so it passes.
  // eslint-disable-next-line no-restricted-properties
  assert.deepEqual(actual, ['hello']);
  const anonWithProp = new (class {})();
  anonWithProp.a = 1;
  assert.throws(
    // eslint-disable-next-line no-restricted-properties
    () => assert.deepEqual(anonWithProp, { a: 2 }),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ/);
      return true;
    },
  );
}

// The hint is also present when a custom message is provided.
{
  const actual = new ExtendedArray('hello');
  const message = 'custom message';
  assert.throws(
    () => assert.deepStrictEqual(actual, ['hello'], message),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(
        err.message,
        /Object prototypes differ: ExtendedArray !== Array/,
      );
      return true;
    },
  );
}

// Deriving the hint may run arbitrary code (here: a `getPrototypeOf` trap of
// a Proxy that throws). That must not replace the ERR_ASSERTION with the trap
// error, must not leak into the message, and must not leave the global
// `Error.stackTraceLimit` modified by the interrupted message construction.
{
  const savedLimit = Error.stackTraceLimit;
  const proxy = new Proxy({}, {
    getPrototypeOf() {
      throw new Error('getPrototypeOf trap');
    },
  });
  assert.throws(
    () => assert.deepStrictEqual(proxy, { x: 1 }),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /getPrototypeOf trap/);
      assert.doesNotMatch(err.message, /Object prototypes differ/);
      return true;
    },
  );
  assert.strictEqual(Error.stackTraceLimit, savedLimit);

  // Constructing the AssertionError directly must behave the same way.
  const err = new assert.AssertionError({
    actual: proxy,
    expected: { x: 1 },
    operator: 'deepStrictEqual',
  });
  assert.strictEqual(err.code, 'ERR_ASSERTION');
  assert.doesNotMatch(err.message, /getPrototypeOf trap/);
  assert.doesNotMatch(err.message, /Object prototypes differ/);
  assert.strictEqual(Error.stackTraceLimit, savedLimit);
}

// The prototype name must be derived from a single read of `constructor.name`
// whose validated value is reused, so that a stateful getter can neither
// change the derived name nor make the message construction throw. The getter
// below returns a different string on every access and a non-convertible
// object from the third access on: the previous implementation read `name`
// three times (twice to validate, once to return) and interpolated the third
// value, escaping its try/catch. Note that util.inspect may also consult the
// constructor name on its own (pre-existing behavior), which accounts for the
// second read.
{
  let reads = 0;
  function Stateful() {}
  Object.defineProperty(Stateful, 'name', {
    get() {
      reads += 1;
      if (reads === 1) {
        return 'Stateful';
      }
      if (reads === 2) {
        return 'SecondRead';
      }
      return {
        [Symbol.toPrimitive]() {
          throw new Error('name coercion');
        },
      };
    },
  });
  const actual = { __proto__: { constructor: Stateful } };
  assert.throws(
    () => assert.deepStrictEqual(actual, {}),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(
        err.message,
        /Object prototypes differ: Stateful !== Object/,
      );
      assert.doesNotMatch(err.message, /SecondRead/);
      assert.doesNotMatch(err.message, /name coercion/);
      return true;
    },
  );
}

// `new assert.Assert({ skipPrototype: true })` intentionally ignores the
// prototypes during the comparison, so the diagnostic must not present the
// ignored difference as the cause of the failure.
{
  class SomeClass {}
  const instance = new SomeClass();
  instance.x = 1;

  // Without the option the hint is present.
  assert.throws(
    () => assert.deepStrictEqual(instance, { x: 2 }),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.match(err.message, /Object prototypes differ: SomeClass !== Object/);
      return true;
    },
  );

  // With `skipPrototype: true` the hint is omitted, while the comparison
  // itself still fails on the differing property.
  const skippingAssert = new assert.Assert({ skipPrototype: true });
  assert.throws(
    () => skippingAssert.deepStrictEqual(instance, { x: 2 }),
    (err) => {
      assert.strictEqual(err.code, 'ERR_ASSERTION');
      assert.doesNotMatch(err.message, /Object prototypes differ/);
      return true;
    },
  );

  // When only the prototypes differ, the skipping instance passes.
  const sameFields = new SomeClass();
  sameFields.x = 1;
  skippingAssert.deepStrictEqual(sameFields, { x: 1 });
}
