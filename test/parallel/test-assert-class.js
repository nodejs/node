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

test('Assert class basic instance', () => {
  const assertInstance = new Assert();

  assertInstance.ok(assert.AssertionError.prototype instanceof Error,
                    'assert.AssertionError instanceof Error');
  assertInstance.ok(true);
  assertInstance.throws(
    () => { assertInstance.fail(); },
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError',
      message: 'Failed',
      operator: 'fail',
      actual: undefined,
      expected: undefined,
      generatedMessage: true,
      stack: /Failed/
    }
  );
  assertInstance.equal(undefined, undefined);
  assertInstance.notEqual(true, false);
  assertInstance.throws(
    () => assertInstance.deepEqual(/a/),
    { code: 'ERR_MISSING_ARGS' }
  );
  assertInstance.throws(
    () => assertInstance.notDeepEqual('test'),
    { code: 'ERR_MISSING_ARGS' }
  );
  assertInstance.notStrictEqual(2, '2');
  assertInstance.throws(() => assertInstance.strictEqual(2, '2'),
                        assertInstance.AssertionError, 'strictEqual(2, \'2\')');
  assertInstance.throws(
    () => {
      assertInstance.partialDeepStrictEqual({ a: true }, { a: false }, 'custom message');
    },
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError',
      message: 'custom message\n+ actual - expected\n\n  {\n+   a: true\n-   a: false\n  }\n'
    }
  );
  assertInstance.throws(
    () => assertInstance.match(/abc/, 'string'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "regexp" argument must be an instance of RegExp. ' +
              "Received type string ('string')"
    }
  );
  assertInstance.throws(
    () => assertInstance.doesNotMatch(/abc/, 'string'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "regexp" argument must be an instance of RegExp. ' +
              "Received type string ('string')"
    }
  );

  /* eslint-disable no-restricted-syntax */
  {
    function thrower(errorConstructor) {
      throw new errorConstructor({});
    }

    let threw = false;
    try {
      assertInstance.doesNotThrow(() => thrower(TypeError), assertInstance.AssertionError);
    } catch (e) {
      threw = true;
      assertInstance.ok(e instanceof TypeError);
    }
    assertInstance.ok(threw, 'assertInstance.doesNotThrow with an explicit error is eating extra errors');
  }
  {
    let threw = false;
    const rangeError = new RangeError('my range');

    try {
      assertInstance.doesNotThrow(() => {
        throw new TypeError('wrong type');
      }, TypeError, rangeError);
    } catch (e) {
      threw = true;
      assertInstance.ok(e.message.includes(rangeError.message));
      assertInstance.ok(e instanceof assertInstance.AssertionError);
      assertInstance.ok(!e.stack.includes('doesNotThrow'), e);
    }
    assertInstance.ok(threw);
  }
  /* eslint-enable no-restricted-syntax */
});
