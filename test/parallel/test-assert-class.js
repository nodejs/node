'use strict';

require('../common');
const assert = require('assert');
const { Assert } = require('assert');
const { inspect } = require('util');
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

test('Assert class with full diff', () => {
  const assertInstance = new Assert({ diff: 'full' });

  const longStringOfAs = 'A'.repeat(1025);
  const longStringOFBs = 'B'.repeat(1025);

  assertInstance.throws(() => {
    assertInstance.strictEqual(longStringOfAs, longStringOFBs);
  }, (err) => {
    assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
    assertInstance.strictEqual(err.message,
                               `Expected values to be strictly equal:\n+ actual - expected\n\n` +
                      `+ '${longStringOfAs}'\n- '${longStringOFBs}'\n`);
    assertInstance.ok(inspect(err).includes(`actual: '${longStringOfAs}'`));
    assertInstance.ok(inspect(err).includes(`expected: '${longStringOFBs}'`));
    return true;
  });

  assertInstance.throws(() => {
    assertInstance.notStrictEqual(longStringOfAs, longStringOfAs);
  }, (err) => {
    assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
    assertInstance.strictEqual(err.message,
                               `Expected "actual" to be strictly unequal to:\n\n` +
      `'${longStringOfAs}'`);
    assertInstance.ok(inspect(err).includes(`actual: '${longStringOfAs}'`));
    assertInstance.ok(inspect(err).includes(`expected: '${longStringOfAs}'`));
    return true;
  });

  assertInstance.throws(() => {
    assertInstance.deepEqual(longStringOfAs, longStringOFBs);
  }, (err) => {
    assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
    assertInstance.strictEqual(
      err.message,
      `Expected values to be loosely deep-equal:\n\n` +
      `'${longStringOfAs}'\n\nshould loosely deep-equal\n\n'${longStringOFBs}'`
    );
    assertInstance.ok(inspect(err).includes(`actual: '${longStringOfAs}'`));
    assertInstance.ok(inspect(err).includes(`expected: '${longStringOFBs}'`));
    return true;
  });
});

test('Assert class with simple diff', () => {
  const assertInstance = new Assert({ diff: 'simple' });

  const longStringOfAs = 'A'.repeat(1025);
  const longStringOFBs = 'B'.repeat(1025);

  assertInstance.throws(() => {
    assertInstance.strictEqual(longStringOfAs, longStringOFBs);
  }, (err) => {
    assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
    assertInstance.strictEqual(err.message,
                               `Expected values to be strictly equal:\n+ actual - expected\n\n` +
                      `+ '${longStringOfAs}'\n- '${longStringOFBs}'\n`);
    assertInstance.ok(inspect(err).includes(`actual: '${longStringOfAs.slice(0, 513)}...`));
    assertInstance.ok(inspect(err).includes(`expected: '${longStringOFBs.slice(0, 513)}...`));
    return true;
  });

  assertInstance.throws(() => {
    assertInstance.notStrictEqual(longStringOfAs, longStringOfAs);
  }, (err) => {
    assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
    assertInstance.strictEqual(err.message,
                               `Expected "actual" to be strictly unequal to:\n\n` +
      `'${longStringOfAs}'`);
    assertInstance.ok(inspect(err).includes(`actual: '${longStringOfAs.slice(0, 513)}...`));
    assertInstance.ok(inspect(err).includes(`expected: '${longStringOfAs.slice(0, 513)}...`));
    return true;
  });

  assertInstance.throws(() => {
    assertInstance.deepEqual(longStringOfAs, longStringOFBs);
  }, (err) => {
    assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
    assertInstance.strictEqual(
      err.message,
      `Expected values to be loosely deep-equal:\n\n` +
      `'${longStringOfAs.slice(0, 508)}...\n\nshould loosely deep-equal\n\n'${longStringOFBs.slice(0, 508)}...`
    );
    assertInstance.ok(inspect(err).includes(`actual: '${longStringOfAs.slice(0, 513)}...`));
    assertInstance.ok(inspect(err).includes(`expected: '${longStringOFBs.slice(0, 513)}...`));
    return true;
  });
});
