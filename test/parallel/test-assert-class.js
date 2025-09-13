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

test('Assert constructor requires new', () => {
  assert.throws(() => Assert(), {
    code: 'ERR_CONSTRUCT_CALL_REQUIRED',
    name: 'TypeError',
  });
});

test('Assert class non strict', () => {
  const assertInstance = new Assert({ diff: undefined, strict: false });

  assertInstance.ok(
    assert.AssertionError.prototype instanceof Error,
    'assert.AssertionError instanceof Error'
  );
  assert.strictEqual(typeof assertInstance.ok, 'function');
  assert.strictEqual(assertInstance.ok.strictEqual, undefined);
  assert.strictEqual(typeof assertInstance.strictEqual, 'function');
  assertInstance.ok(true);
  assertInstance.throws(
    () => {
      assertInstance.fail();
    },
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError',
      message: 'Failed',
      operator: 'fail',
      actual: undefined,
      expected: undefined,
      generatedMessage: true,
      stack: /Failed/,
    }
  );
  assertInstance.equal(undefined, undefined);
  assertInstance.equal(null, undefined);
  assertInstance.equal(2, '2');
  assertInstance.notEqual(true, false);
  assertInstance.throws(() => assertInstance.deepEqual(/a/), {
    code: 'ERR_MISSING_ARGS',
  });
  assertInstance.throws(() => assertInstance.notDeepEqual('test'), {
    code: 'ERR_MISSING_ARGS',
  });
  assertInstance.notStrictEqual(2, '2');
  assertInstance.throws(
    () => assertInstance.strictEqual(2, '2'),
    assertInstance.AssertionError,
    "strictEqual(2, '2')"
  );
  assertInstance.throws(
    () => {
      assertInstance.partialDeepStrictEqual(
        { a: true },
        { a: false },
        'custom message'
      );
    },
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError',
      message:
        'custom message\n+ actual - expected\n\n  {\n+   a: true\n-   a: false\n  }\n',
    }
  );
  assertInstance.throws(() => assertInstance.match(/abc/, 'string'), {
    code: 'ERR_INVALID_ARG_TYPE',
    message:
      'The "regexp" argument must be an instance of RegExp. ' +
      "Received type string ('string')",
  });
  assertInstance.throws(() => assertInstance.doesNotMatch(/abc/, 'string'), {
    code: 'ERR_INVALID_ARG_TYPE',
    message:
      'The "regexp" argument must be an instance of RegExp. ' +
      "Received type string ('string')",
  });

  /* eslint-disable no-restricted-syntax */
  {
    function thrower(errorConstructor) {
      throw new errorConstructor({});
    }

    let threw = false;
    try {
      assertInstance.doesNotThrow(
        () => thrower(TypeError),
        assertInstance.AssertionError
      );
    } catch (e) {
      threw = true;
      assertInstance.ok(e instanceof TypeError);
    }
    assertInstance.ok(
      threw,
      'assertInstance.doesNotThrow with an explicit error is eating extra errors'
    );
  }
  {
    let threw = false;
    const rangeError = new RangeError('my range');

    try {
      assertInstance.doesNotThrow(
        () => {
          throw new TypeError('wrong type');
        },
        TypeError,
        rangeError
      );
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

test('Assert class strict', () => {
  const assertInstance = new Assert();

  assertInstance.equal(assertInstance.equal, assertInstance.strictEqual);
  assertInstance.equal(
    assertInstance.deepEqual,
    assertInstance.deepStrictEqual
  );
  assertInstance.equal(assertInstance.notEqual, assertInstance.notStrictEqual);
  assertInstance.equal(
    assertInstance.notDeepEqual,
    assertInstance.notDeepStrictEqual
  );
});

test('Assert class with invalid diff option', () => {
  assert.throws(() => new Assert({ diff: 'invalid' }), {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
    message: "The property 'options.diff' must be one of: 'simple', 'full'. Received 'invalid'",
  });
});

const longLinesOfAs = 'A\n'.repeat(100);
const longLinesOFBs = 'B\n'.repeat(100);
const truncatedAs = 'A\\n'.repeat(10) + '...';
const truncatedBs = 'B\\n'.repeat(10) + '...';

const longStringOfAs = 'A'.repeat(10_000);
const longStringOfBs = 'B'.repeat(10_000);

const longLinesOfAsWithEllipsis = longStringOfAs.substring(0, 9_488) + '...';
const longLinesOFBsWithEllipsis = longStringOfBs.substring(0, 9_488) + '...';
test('Assert class non strict with full diff', () => {
  const assertInstance = new Assert({ diff: 'full', strict: false });

  // long strings
  {
    assertInstance.throws(
      () => {
        assertInstance.strictEqual(longStringOfAs, longStringOfBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'strictEqual');
        assertInstance.strictEqual(err.diff, 'full');
        assertInstance.strictEqual(err.actual, longStringOfAs);
        assertInstance.strictEqual(err.expected, longStringOfBs);

        assertInstance.strictEqual(
          err.message,
          `Expected values to be strictly equal:\n+ actual - expected\n\n` +
            `+ '${longStringOfAs}'\n- '${longStringOfBs}'\n`
        );
        assertInstance.ok(
          inspect(err).includes(`actual: '${longLinesOfAsWithEllipsis}'`)
        );
        assertInstance.ok(
          inspect(err).includes(`expected: '${longLinesOFBsWithEllipsis}'`)
        );
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.notStrictEqual(longStringOfAs, longStringOfAs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'notStrictEqual');
        assertInstance.strictEqual(err.diff, 'full');
        assertInstance.strictEqual(err.actual, longStringOfAs);
        assertInstance.strictEqual(err.expected, longStringOfAs);

        assertInstance.strictEqual(
          err.message,
          `Expected "actual" to be strictly unequal to:\n\n` +
            `'${longStringOfAs}'`
        );
        assertInstance.ok(
          inspect(err).includes(`actual: '${longLinesOfAsWithEllipsis}'`)
        );
        assertInstance.ok(
          inspect(err).includes(`expected: '${longLinesOfAsWithEllipsis}'`)
        );
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.deepEqual(longStringOfAs, longStringOfBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'deepEqual');
        assertInstance.strictEqual(err.diff, 'full');
        assertInstance.strictEqual(err.actual, longStringOfAs);
        assertInstance.strictEqual(err.expected, longStringOfBs);

        assertInstance.strictEqual(
          err.message,
          `Expected values to be loosely deep-equal:\n\n` +
            `'${longStringOfAs}'\n\nshould loosely deep-equal\n\n'${longStringOfBs}'`
        );
        assertInstance.ok(
          inspect(err).includes(`actual: '${longLinesOfAsWithEllipsis}'`)
        );
        assertInstance.ok(
          inspect(err).includes(`expected: '${longLinesOFBsWithEllipsis}'`)
        );
        return true;
      }
    );
  }

  // long lines
  {
    assertInstance.throws(
      () => {
        assertInstance.strictEqual(longLinesOfAs, longLinesOFBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'strictEqual');
        assertInstance.strictEqual(err.diff, 'full');
        assertInstance.strictEqual(err.actual, longLinesOfAs);
        assertInstance.strictEqual(err.expected, longLinesOFBs);

        assertInstance.strictEqual(err.message.split('\n').length, 204);
        assertInstance.strictEqual(err.actual.split('\n').length, 101);
        assertInstance.ok(
          err.message.includes('Expected values to be strictly equal')
        );
        assertInstance.ok(inspect(err).includes(`actual: '${truncatedAs}`));
        assertInstance.ok(inspect(err).includes(`expected: '${truncatedBs}`));
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.notStrictEqual(longLinesOfAs, longLinesOfAs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'notStrictEqual');
        assertInstance.strictEqual(err.diff, 'full');
        assertInstance.strictEqual(err.actual, longLinesOfAs);
        assertInstance.strictEqual(err.expected, longLinesOfAs);

        assertInstance.strictEqual(err.message.split('\n').length, 103);
        assertInstance.strictEqual(err.actual.split('\n').length, 101);
        assertInstance.ok(
          err.message.includes(`Expected "actual" to be strictly unequal to:`)
        );
        assertInstance.ok(inspect(err).includes(`actual: '${truncatedAs}`));
        assertInstance.ok(inspect(err).includes(`expected: '${truncatedAs}`));
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.deepEqual(longLinesOfAs, longLinesOFBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'deepEqual');
        assertInstance.strictEqual(err.diff, 'full');
        assertInstance.strictEqual(err.actual, longLinesOfAs);
        assertInstance.strictEqual(err.expected, longLinesOFBs);

        assertInstance.strictEqual(err.message.split('\n').length, 205);
        assertInstance.strictEqual(err.actual.split('\n').length, 101);
        assertInstance.ok(
          err.message.includes(`Expected values to be loosely deep-equal:`)
        );
        assertInstance.ok(inspect(err).includes(`actual: '${truncatedAs}`));
        assertInstance.ok(inspect(err).includes(`expected: '${truncatedBs}`));
        return true;
      }
    );
  }
});

test('Assert class non strict with simple diff', () => {
  const assertInstance = new Assert({ diff: 'simple', strict: false });

  // long strings
  {
    assertInstance.throws(
      () => {
        assertInstance.strictEqual(longStringOfAs, longStringOfBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'strictEqual');
        assertInstance.strictEqual(err.diff, 'simple');
        assertInstance.strictEqual(err.actual, longStringOfAs);
        assertInstance.strictEqual(err.expected, longStringOfBs);

        assertInstance.strictEqual(
          err.message,
          `Expected values to be strictly equal:\n+ actual - expected\n\n` +
            `+ '${longStringOfAs}'\n- '${longStringOfBs}'\n`
        );
        assertInstance.ok(
          inspect(err).includes(`actual: '${longLinesOfAsWithEllipsis}'`)
        );
        assertInstance.ok(
          inspect(err).includes(`expected: '${longLinesOFBsWithEllipsis}'`)
        );
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.notStrictEqual(longStringOfAs, longStringOfAs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'notStrictEqual');
        assertInstance.strictEqual(err.diff, 'simple');
        assertInstance.strictEqual(err.actual, longStringOfAs);
        assertInstance.strictEqual(err.expected, longStringOfAs);

        assertInstance.strictEqual(
          err.message,
          `Expected "actual" to be strictly unequal to:\n\n` +
            `'${longStringOfAs}'`
        );
        assertInstance.ok(
          inspect(err).includes(`actual: '${longLinesOfAsWithEllipsis}'`)
        );
        assertInstance.ok(
          inspect(err).includes(`expected: '${longLinesOfAsWithEllipsis}'`)
        );
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.deepEqual(longStringOfAs, longStringOfBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'deepEqual');
        assertInstance.strictEqual(err.diff, 'simple');
        assertInstance.strictEqual(err.actual, longStringOfAs);
        assertInstance.strictEqual(err.expected, longStringOfBs);

        assertInstance.strictEqual(
          err.message,
          `Expected values to be loosely deep-equal:\n\n` +
            `'${
              longStringOfAs.substring(0, 508) + '...'
            }\n\nshould loosely deep-equal\n\n'${
              longStringOfBs.substring(0, 508) + '...'
            }`
        );
        assertInstance.ok(
          inspect(err).includes(`actual: '${longLinesOfAsWithEllipsis}'`)
        );
        assertInstance.ok(
          inspect(err).includes(`expected: '${longLinesOFBsWithEllipsis}'`)
        );
        return true;
      }
    );
  }

  // long lines
  {
    assertInstance.throws(
      () => {
        assertInstance.strictEqual(longLinesOfAs, longLinesOFBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'strictEqual');
        assertInstance.strictEqual(err.diff, 'simple');
        assertInstance.strictEqual(err.actual, longLinesOfAs);
        assertInstance.strictEqual(err.expected, longLinesOFBs);
        assertInstance.strictEqual(err.message.split('\n').length, 204);
        assertInstance.strictEqual(err.actual.split('\n').length, 101);

        assertInstance.ok(
          err.message.includes('Expected values to be strictly equal')
        );
        assertInstance.ok(inspect(err).includes(`actual: '${truncatedAs}`));
        assertInstance.ok(inspect(err).includes(`expected: '${truncatedBs}`));
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.notStrictEqual(longLinesOfAs, longLinesOfAs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'notStrictEqual');
        assertInstance.strictEqual(err.diff, 'simple');
        assertInstance.strictEqual(err.actual, longLinesOfAs);
        assertInstance.strictEqual(err.expected, longLinesOfAs);

        assertInstance.strictEqual(err.message.split('\n').length, 50);
        assertInstance.strictEqual(err.actual.split('\n').length, 101);
        assertInstance.ok(
          err.message.includes(`Expected "actual" to be strictly unequal to:`)
        );
        assertInstance.ok(inspect(err).includes(`actual: '${truncatedAs}`));
        assertInstance.ok(inspect(err).includes(`expected: '${truncatedAs}`));
        return true;
      }
    );

    assertInstance.throws(
      () => {
        assertInstance.deepEqual(longLinesOfAs, longLinesOFBs);
      },
      (err) => {
        assertInstance.strictEqual(err.code, 'ERR_ASSERTION');
        assertInstance.strictEqual(err.operator, 'deepEqual');
        assertInstance.strictEqual(err.diff, 'simple');
        assertInstance.strictEqual(err.actual, longLinesOfAs);
        assertInstance.strictEqual(err.expected, longLinesOFBs);

        assertInstance.strictEqual(err.message.split('\n').length, 109);
        assertInstance.strictEqual(err.actual.split('\n').length, 101);
        assertInstance.ok(
          err.message.includes(`Expected values to be loosely deep-equal:`)
        );
        assertInstance.ok(inspect(err).includes(`actual: '${truncatedAs}`));
        assertInstance.ok(inspect(err).includes(`expected: '${truncatedBs}`));
        return true;
      }
    );
  }
});

// Shared setup for skipPrototype tests
{
  const message = 'Expected values to be strictly deep-equal:\n' +
  '+ actual - expected\n' +
  '\n' +
  '  [\n' +
  '    1,\n' +
  '    2,\n' +
  '    3,\n' +
  '    4,\n' +
  '    5,\n' +
  '+   6,\n' +
  '-   9,\n' +
  '    7\n' +
  '  ]\n';

  function CoolClass(name) { this.name = name; }

  function AwesomeClass(name) { this.name = name; }

  class Modern { constructor(value) { this.value = value; } }
  class Legacy { constructor(value) { this.value = value; } }

  const cool = new CoolClass('Assert is inspiring');
  const awesome = new AwesomeClass('Assert is inspiring');
  const modern = new Modern(42);
  const legacy = new Legacy(42);

  test('Assert class strict with skipPrototype', () => {
    const assertInstance = new Assert({ skipPrototype: true });

    assert.throws(
      () => assertInstance.deepEqual([1, 2, 3, 4, 5, 6, 7], [1, 2, 3, 4, 5, 9, 7]),
      { message }
    );

    assertInstance.deepEqual(cool, awesome);
    assertInstance.deepStrictEqual(cool, awesome);
    assertInstance.deepEqual(modern, legacy);
    assertInstance.deepStrictEqual(modern, legacy);

    const cool2 = new CoolClass('Soooo coooool');
    assert.throws(
      () => assertInstance.deepStrictEqual(cool, cool2),
      { code: 'ERR_ASSERTION' }
    );

    const nested1 = { obj: new CoolClass('test'), arr: [1, 2, 3] };
    const nested2 = { obj: new AwesomeClass('test'), arr: [1, 2, 3] };
    assertInstance.deepStrictEqual(nested1, nested2);

    const arr = new Uint8Array([1, 2, 3]);
    const buf = Buffer.from([1, 2, 3]);
    assertInstance.deepStrictEqual(arr, buf);
  });

  test('Assert class non strict with skipPrototype', () => {
    const assertInstance = new Assert({ strict: false, skipPrototype: true });

    assert.throws(
      () => assertInstance.deepStrictEqual([1, 2, 3, 4, 5, 6, 7], [1, 2, 3, 4, 5, 9, 7]),
      { message }
    );

    assertInstance.deepStrictEqual(cool, awesome);
    assertInstance.deepStrictEqual(modern, legacy);
  });

  test('Assert class skipPrototype with complex objects', () => {
    const assertInstance = new Assert({ skipPrototype: true });

    function ComplexAwesomeClass(name, age) {
      this.name = name;
      this.age = age;
      this.settings = {
        theme: 'dark',
        lang: 'en'
      };
    }

    function ComplexCoolClass(name, age) {
      this.name = name;
      this.age = age;
      this.settings = {
        theme: 'dark',
        lang: 'en'
      };
    }

    const awesome1 = new ComplexAwesomeClass('Foo', 30);
    const cool1 = new ComplexCoolClass('Foo', 30);

    assertInstance.deepStrictEqual(awesome1, cool1);

    const cool2 = new ComplexCoolClass('Foo', 30);
    cool2.settings.theme = 'light';

    assert.throws(
      () => assertInstance.deepStrictEqual(awesome1, cool2),
      { code: 'ERR_ASSERTION' }
    );
  });

  test('Assert class skipPrototype with arrays and special objects', () => {
    const assertInstance = new Assert({ skipPrototype: true });

    const arr1 = [1, 2, 3];
    const arr2 = new Array(1, 2, 3);
    assertInstance.deepStrictEqual(arr1, arr2);

    const date1 = new Date('2023-01-01');
    const date2 = new Date('2023-01-01');
    assertInstance.deepStrictEqual(date1, date2);

    const regex1 = /test/g;
    const regex2 = new RegExp('test', 'g');
    assertInstance.deepStrictEqual(regex1, regex2);

    const date3 = new Date('2023-01-02');
    assert.throws(
      () => assertInstance.deepStrictEqual(date1, date3),
      { code: 'ERR_ASSERTION' }
    );
  });

  test('Assert class skipPrototype with notDeepStrictEqual', () => {
    const assertInstance = new Assert({ skipPrototype: true });

    assert.throws(
      () => assertInstance.notDeepStrictEqual(cool, awesome),
      { code: 'ERR_ASSERTION' }
    );

    const notAwesome = new AwesomeClass('Not so awesome');
    assertInstance.notDeepStrictEqual(cool, notAwesome);

    const defaultAssertInstance = new Assert({ skipPrototype: false });
    defaultAssertInstance.notDeepStrictEqual(cool, awesome);
  });

  test('Assert class skipPrototype with mixed types', () => {
    const assertInstance = new Assert({ skipPrototype: true });

    const obj1 = { value: 42, nested: { prop: 'test' } };

    function CustomObj(value, nested) {
      this.value = value;
      this.nested = nested;
    }

    const obj2 = new CustomObj(42, { prop: 'test' });
    assertInstance.deepStrictEqual(obj1, obj2);

    assert.throws(
      () => assertInstance.deepStrictEqual({ num: 42 }, { num: '42' }),
      { code: 'ERR_ASSERTION' }
    );
  });
}
