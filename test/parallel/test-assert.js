// Flags: --expose-internals
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');
const assert = require('assert');
const { inspect } = require('util');
const { internalBinding } = require('internal/test/binding');
const a = assert;

// Disable colored output to prevent color codes from breaking assertion
// message comparisons. This should only be an issue when process.stdout
// is a TTY.
if (process.stdout.isTTY)
  process.env.NODE_DISABLE_COLORS = '1';

const strictEqualMessageStart = 'Expected values to be strictly equal:\n';
const start = 'Expected values to be strictly deep-equal:';
const actExp = '+ actual - expected';

assert.ok(a.AssertionError.prototype instanceof Error,
          'a.AssertionError instanceof Error');

assert.throws(() => a(false), a.AssertionError, 'ok(false)');
assert.throws(() => a.ok(false), a.AssertionError, 'ok(false)');

a(true);
a('test', 'ok(\'test\')');
a.ok(true);
a.ok('test');

assert.throws(() => a.equal(true, false),
              a.AssertionError, 'equal(true, false)');

a.equal(null, null);
a.equal(undefined, undefined);
a.equal(null, undefined);
a.equal(true, true);
a.equal(2, '2');
a.notEqual(true, false);

assert.throws(() => a.notEqual(true, true),
              a.AssertionError, 'notEqual(true, true)');

assert.throws(() => a.strictEqual(2, '2'),
              a.AssertionError, 'strictEqual(2, \'2\')');

/* eslint-disable no-restricted-syntax */
assert.throws(() => a.strictEqual(null, undefined),
              a.AssertionError, 'strictEqual(null, undefined)');

assert.throws(
  () => a.notStrictEqual(2, 2),
  {
    message: 'Expected "actual" to be strictly unequal to: 2',
    name: 'AssertionError [ERR_ASSERTION]'
  }
);

assert.throws(
  () => a.notStrictEqual('a '.repeat(30), 'a '.repeat(30)),
  {
    message: 'Expected "actual" to be strictly unequal to: ' +
             `'${'a '.repeat(30)}'`,
    name: 'AssertionError [ERR_ASSERTION]'
  }
);

a.notStrictEqual(2, '2');

// Testing the throwing.
function thrower(errorConstructor) {
  throw new errorConstructor({});
}

// The basic calls work.
assert.throws(() => thrower(a.AssertionError), a.AssertionError, 'message');
assert.throws(() => thrower(a.AssertionError), a.AssertionError);
assert.throws(() => thrower(a.AssertionError));

// If not passing an error, catch all.
assert.throws(() => thrower(TypeError));

// When passing a type, only catch errors of the appropriate type.
{
  let threw = false;
  try {
    a.throws(() => thrower(TypeError), a.AssertionError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof TypeError, 'type');
  }
  assert.ok(threw, 'a.throws with an explicit error is eating extra errors');
}

// doesNotThrow should pass through all errors.
{
  let threw = false;
  try {
    a.doesNotThrow(() => thrower(TypeError), a.AssertionError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof TypeError);
  }
  assert(threw, 'a.doesNotThrow with an explicit error is eating extra errors');
}

// Key difference is that throwing our correct error makes an assertion error.
{
  let threw = false;
  try {
    a.doesNotThrow(() => thrower(TypeError), TypeError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof a.AssertionError);
    assert.ok(!e.stack.includes('at Function.doesNotThrow'));
  }
  assert.ok(threw, 'a.doesNotThrow is not catching type matching errors');
}

assert.throws(
  () => a.doesNotThrow(() => thrower(Error), 'user message'),
  {
    name: 'AssertionError [ERR_ASSERTION]',
    code: 'ERR_ASSERTION',
    operator: 'doesNotThrow',
    message: 'Got unwanted exception: user message\n' +
             'Actual message: "[object Object]"'
  }
);

assert.throws(
  () => a.doesNotThrow(() => thrower(Error)),
  {
    code: 'ERR_ASSERTION',
    message: 'Got unwanted exception.\nActual message: "[object Object]"'
  }
);

// Make sure that validating using constructor really works.
{
  let threw = false;
  try {
    assert.throws(
      () => {
        throw ({}); // eslint-disable-line no-throw-literal
      },
      Array
    );
  } catch {
    threw = true;
  }
  assert.ok(threw, 'wrong constructor validation');
}

// Use a RegExp to validate the error message.
a.throws(() => thrower(TypeError), /\[object Object\]/);

// Use a fn to validate the error object.
a.throws(() => thrower(TypeError), (err) => {
  if ((err instanceof TypeError) && /\[object Object\]/.test(err)) {
    return true;
  }
});

// https://github.com/nodejs/node/issues/3188
{
  let threw = false;
  let AnotherErrorType;
  try {
    const ES6Error = class extends Error {};
    AnotherErrorType = class extends Error {};

    assert.throws(() => { throw new AnotherErrorType('foo'); }, ES6Error);
  } catch (e) {
    threw = true;
    assert(e instanceof AnotherErrorType,
           `expected AnotherErrorType, received ${e}`);
  }

  assert.ok(threw);
}

// Check messages from assert.throws().
{
  const noop = () => {};
  assert.throws(
    () => { a.throws((noop)); },
    {
      code: 'ERR_ASSERTION',
      message: 'Missing expected exception.',
      operator: 'throws',
      actual: undefined,
      expected: undefined
    });

  assert.throws(
    () => { a.throws(noop, TypeError); },
    {
      code: 'ERR_ASSERTION',
      message: 'Missing expected exception (TypeError).',
      actual: undefined,
      expected: TypeError
    });

  assert.throws(
    () => { a.throws(noop, 'fhqwhgads'); },
    {
      code: 'ERR_ASSERTION',
      message: 'Missing expected exception: fhqwhgads',
      actual: undefined,
      expected: undefined
    });

  assert.throws(
    () => { a.throws(noop, TypeError, 'fhqwhgads'); },
    {
      code: 'ERR_ASSERTION',
      message: 'Missing expected exception (TypeError): fhqwhgads',
      actual: undefined,
      expected: TypeError
    });

  let threw = false;
  try {
    a.throws(noop);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof a.AssertionError);
    assert.ok(!e.stack.includes('at Function.throws'));
  }
  assert.ok(threw);
}

const circular = { y: 1 };
circular.x = circular;

function testAssertionMessage(actual, expected, msg) {
  assert.throws(
    () => assert.strictEqual(actual, ''),
    {
      generatedMessage: true,
      message: msg || strictEqualMessageStart +
               `+ actual - expected\n\n+ ${expected}\n- ''`
    }
  );
}

function testShortAssertionMessage(actual, expected) {
  testAssertionMessage(actual, expected, strictEqualMessageStart +
                                         `\n${inspect(actual)} !== ''\n`);
}

testShortAssertionMessage(null, 'null');
testShortAssertionMessage(true, 'true');
testShortAssertionMessage(false, 'false');
testShortAssertionMessage(100, '100');
testShortAssertionMessage(NaN, 'NaN');
testShortAssertionMessage(Infinity, 'Infinity');
testShortAssertionMessage('a', '"a"');
testShortAssertionMessage('foo', '\'foo\'');
testShortAssertionMessage(0, '0');
testShortAssertionMessage(Symbol(), 'Symbol()');
testAssertionMessage([], '[]');
testAssertionMessage(/a/, '/a/');
testAssertionMessage(/abc/gim, '/abc/gim');
testAssertionMessage({}, '{}');
testAssertionMessage(undefined, 'undefined');
testAssertionMessage(-Infinity, '-Infinity');
testAssertionMessage([1, 2, 3], '[\n+   1,\n+   2,\n+   3\n+ ]');
testAssertionMessage(function f() {}, '[Function: f]');
testAssertionMessage(function() {}, '[Function]');
testAssertionMessage(circular, '{\n+   x: [Circular],\n+   y: 1\n+ }');
testAssertionMessage({ a: undefined, b: null },
                     '{\n+   a: undefined,\n+   b: null\n+ }');
testAssertionMessage({ a: NaN, b: Infinity, c: -Infinity },
                     '{\n+   a: NaN,\n+   b: Infinity,\n+   c: -Infinity\n+ }');

// https://github.com/nodejs/node-v0.x-archive/issues/5292
try {
  assert.strictEqual(1, 2);
} catch (e) {
  assert.strictEqual(
    e.message,
    `${strictEqualMessageStart}\n1 !== 2\n`
  );
  assert.ok(e.generatedMessage, 'Message not marked as generated');
}

try {
  assert.strictEqual(1, 2, 'oh no');
} catch (e) {
  assert.strictEqual(e.message, 'oh no');
  // Message should not be marked as generated.
  assert.strictEqual(e.generatedMessage, false);
}

{
  let threw = false;
  const rangeError = new RangeError('my range');

  // Verify custom errors.
  try {
    assert.strictEqual(1, 2, rangeError);
  } catch (e) {
    assert.strictEqual(e, rangeError);
    threw = true;
    assert.ok(e instanceof RangeError, 'Incorrect error type thrown');
  }
  assert.ok(threw);
  threw = false;

  // Verify AssertionError is the result from doesNotThrow with custom Error.
  try {
    a.doesNotThrow(() => {
      throw new TypeError('wrong type');
    }, TypeError, rangeError);
  } catch (e) {
    threw = true;
    assert.ok(e.message.includes(rangeError.message));
    assert.ok(e instanceof assert.AssertionError);
    assert.ok(!e.stack.includes('doesNotThrow'), e.stack);
  }
  assert.ok(threw);
}

{
  // Verify that throws() and doesNotThrow() throw on non-functions.
  const testBlockTypeError = (method, fn) => {
    common.expectsError(
      () => method(fn),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "fn" argument must be of type Function. Received ' +
                 `type ${typeof fn}`
      }
    );
  };

  testBlockTypeError(assert.throws, 'string');
  testBlockTypeError(assert.doesNotThrow, 'string');
  testBlockTypeError(assert.throws, 1);
  testBlockTypeError(assert.doesNotThrow, 1);
  testBlockTypeError(assert.throws, true);
  testBlockTypeError(assert.doesNotThrow, true);
  testBlockTypeError(assert.throws, false);
  testBlockTypeError(assert.doesNotThrow, false);
  testBlockTypeError(assert.throws, []);
  testBlockTypeError(assert.doesNotThrow, []);
  testBlockTypeError(assert.throws, {});
  testBlockTypeError(assert.doesNotThrow, {});
  testBlockTypeError(assert.throws, /foo/);
  testBlockTypeError(assert.doesNotThrow, /foo/);
  testBlockTypeError(assert.throws, null);
  testBlockTypeError(assert.doesNotThrow, null);
  testBlockTypeError(assert.throws, undefined);
  testBlockTypeError(assert.doesNotThrow, undefined);
}

// https://github.com/nodejs/node/issues/3275
// eslint-disable-next-line no-throw-literal
assert.throws(() => { throw 'error'; }, (err) => err === 'error');
assert.throws(() => { throw new Error(); }, (err) => err instanceof Error);

// Long values should be truncated for display.
assert.throws(() => {
  assert.strictEqual('A'.repeat(1000), '');
}, {
  code: 'ERR_ASSERTION',
  message: `${strictEqualMessageStart}+ actual - expected\n\n` +
           `+ '${'A'.repeat(1000)}'\n- ''`
});

{
  // Bad args to AssertionError constructor should throw TypeError.
  const args = [1, true, false, '', null, Infinity, Symbol('test'), undefined];
  args.forEach((input) => {
    assert.throws(
      () => new assert.AssertionError(input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError [ERR_INVALID_ARG_TYPE]',
        message: 'The "options" argument must be of type Object. ' +
                 `Received type ${typeof input}`
      });
  });
}

assert.throws(
  () => assert.strictEqual(new Error('foo'), new Error('foobar')),
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError [ERR_ASSERTION]',
    message: 'Expected "actual" to be reference-equal to "expected":\n' +
             '+ actual - expected\n\n' +
             '+ [Error: foo]\n- [Error: foobar]'
  }
);

// Test strict assert.
{
  const a = require('assert');
  const assert = require('assert').strict;
  /* eslint-disable no-restricted-properties */
  assert.throws(() => assert.equal(1, true), assert.AssertionError);
  assert.notEqual(0, false);
  assert.throws(() => assert.deepEqual(1, true), assert.AssertionError);
  assert.notDeepEqual(0, false);
  assert.equal(assert.strict, assert.strict.strict);
  assert.equal(assert.equal, assert.strictEqual);
  assert.equal(assert.deepEqual, assert.deepStrictEqual);
  assert.equal(assert.notEqual, assert.notStrictEqual);
  assert.equal(assert.notDeepEqual, assert.notDeepStrictEqual);
  assert.equal(Object.keys(assert).length, Object.keys(a).length);
  assert(7);
  assert.throws(
    () => assert(...[]),
    {
      message: 'No value argument passed to `assert.ok()`',
      name: 'AssertionError [ERR_ASSERTION]',
      generatedMessage: true
    }
  );
  assert.throws(
    () => a(),
    {
      message: 'No value argument passed to `assert.ok()`',
      name: 'AssertionError [ERR_ASSERTION]'
    }
  );

  // Test setting the limit to zero and that assert.strict works properly.
  const tmpLimit = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  common.expectsError(
    () => {
      assert.ok(
        typeof 123 === 'string'
      );
    },
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'The expression evaluated to a falsy value:\n\n  ' +
               "assert.ok(\n    typeof 123 === 'string'\n  )\n"
    }
  );
  Error.stackTraceLimit = tmpLimit;

  // Test error diffs.
  let message = [
    start,
    `${actExp} ... Lines skipped`,
    '',
    '  [',
    '    [',
    '...',
    '        2,',
    '+       3',
    "-       '3'",
    '      ]',
    '...',
    '    5',
    '  ]'].join('\n');
  assert.throws(
    () => assert.deepEqual([[[1, 2, 3]], 4, 5], [[[1, 2, '3']], 4, 5]),
    { message });

  message = [
    start,
    `${actExp} ... Lines skipped`,
    '',
    '  [',
    '    1,',
    '...',
    '    0,',
    '-   1,',
    '    1,',
    '...',
    '    1',
    '  ]'
  ].join('\n');
  assert.throws(
    () => assert.deepEqual(
      [1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1],
      [1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1]),
    { message });

  message = [
    start,
    `${actExp} ... Lines skipped`,
    '',
    '  [',
    '    1,',
    '...',
    '    0,',
    '+   1,',
    '    1,',
    '...',
    '    1',
    '  ]'
  ].join('\n');
  assert.throws(
    () => assert.deepEqual(
      [1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1],
      [1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1]),
    { message });

  message = [
    start,
    actExp,
    '',
    '  [',
    '    1,',
    '+   2,',
    '-   1,',
    '    1,',
    '    1,',
    '    0,',
    '+   1,',
    '    1',
    '  ]'
  ].join('\n');
  assert.throws(
    () => assert.deepEqual(
      [1, 2, 1, 1, 0, 1, 1],
      [1, 1, 1, 1, 0, 1]),
    { message });

  message = [
    start,
    actExp,
    '',
    '+ [',
    '+   1,',
    '+   2,',
    '+   1',
    '+ ]',
    '- undefined',
  ].join('\n');
  assert.throws(
    () => assert.deepEqual([1, 2, 1]),
    { message });

  message = [
    start,
    actExp,
    '',
    '  [',
    '+   1,',
    '    2,',
    '    1',
    '  ]'
  ].join('\n');
  assert.throws(
    () => assert.deepEqual([1, 2, 1], [2, 1]),
    { message });

  message = `${start}\n` +
    `${actExp} ... Lines skipped\n` +
    '\n' +
    '  [\n' +
    '+   1,\n'.repeat(10) +
    '...\n' +
    '-   2,\n'.repeat(10) +
    '...';
  assert.throws(
    () => assert.deepEqual(Array(12).fill(1), Array(12).fill(2)),
    { message });

  const obj1 = {};
  const obj2 = { loop: 'forever' };
  obj2[inspect.custom] = () => '{}';
  // No infinite loop and no custom inspect.
  assert.throws(() => assert.deepEqual(obj1, obj2), {
    message: `${start}\n` +
    `${actExp}\n` +
    '\n' +
    '+ {}\n' +
    '- {\n' +
    '-   [Symbol(nodejs.util.inspect.custom)]: [Function],\n' +
    "-   loop: 'forever'\n" +
    '- }'
  });

  // notDeepEqual tests
  assert.throws(
    () => assert.notDeepEqual([1], [1]),
    {
      message: 'Expected "actual" not to be strictly deep-equal to:\n\n' +
               '[\n  1\n]\n'
    }
  );

  message = 'Expected "actual" not to be strictly deep-equal to:' +
            `\n\n[${'\n  1,'.repeat(25)}\n...\n`;
  const data = Array(31).fill(1);
  assert.throws(
    () => assert.notDeepEqual(data, data),
    { message });
  /* eslint-enable no-restricted-properties */
}

common.expectsError(
  () => assert.ok(null),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    generatedMessage: true,
    message: 'The expression evaluated to a falsy value:\n\n  ' +
             'assert.ok(null)\n'
  }
);
common.expectsError(
  () => assert(typeof 123 === 'string'),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    generatedMessage: true,
    message: 'The expression evaluated to a falsy value:\n\n  ' +
             "assert(typeof 123 === 'string')\n"
  }
);

common.expectsError(
  () => assert(false, Symbol('foo')),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    generatedMessage: false,
    message: 'Symbol(foo)'
  }
);

{
  // Test caching.
  const fs = internalBinding('fs');
  const tmp = fs.close;
  fs.close = common.mustCall(tmp, 1);
  function throwErr() {
    assert(
      (Buffer.from('test') instanceof Error)
    );
  }
  common.expectsError(
    () => throwErr(),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'The expression evaluated to a falsy value:\n\n  ' +
               "assert(\n    (Buffer.from('test') instanceof Error)\n  )\n"
    }
  );
  common.expectsError(
    () => throwErr(),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'The expression evaluated to a falsy value:\n\n  ' +
               "assert(\n    (Buffer.from('test') instanceof Error)\n  )\n"
    }
  );
  fs.close = tmp;
}

common.expectsError(
  () => {
    a(
      (() => 'string')()
      // eslint-disable-next-line operator-linebreak
      ===
      123 instanceof
          Buffer
    );
  },
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n' +
             '  a(\n' +
             '    (() => \'string\')()\n' +
             '    // eslint-disable-next-line operator-linebreak\n' +
             '    ===\n' +
             '    123 instanceof\n' +
             '        Buffer\n' +
             '  )\n'
  }
);

common.expectsError(
  () => {
    a(
      (() => 'string')()
      // eslint-disable-next-line operator-linebreak
      ===
  123 instanceof
          Buffer
    );
  },
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n' +
             '  a(\n' +
             '    (() => \'string\')()\n' +
             '    // eslint-disable-next-line operator-linebreak\n' +
             '    ===\n' +
             '  123 instanceof\n' +
             '        Buffer\n' +
             '  )\n'
  }
);

/* eslint-disable indent */
common.expectsError(() => {
a((
  () => 'string')() ===
123 instanceof
Buffer
);
}, {
  code: 'ERR_ASSERTION',
  type: assert.AssertionError,
  message: 'The expression evaluated to a falsy value:\n\n' +
           '  a((\n' +
           '    () => \'string\')() ===\n' +
           '  123 instanceof\n' +
           '  Buffer\n' +
           '  )\n'
  }
);
/* eslint-enable indent */

common.expectsError(
  () => {
    assert(true); assert(null, undefined);
  },
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n  ' +
             'assert(null, undefined)\n'
  }
);

common.expectsError(
  () => {
    assert
     .ok(null, undefined);
  },
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n  ' +
             'ok(null, undefined)\n'
  }
);

common.expectsError(
  // eslint-disable-next-line dot-notation, quotes
  () => assert['ok']["apply"](null, [0]),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n  ' +
             'assert[\'ok\']["apply"](null, [0])\n'
  }
);

common.expectsError(
  () => {
    const wrapper = (fn, value) => fn(value);
    wrapper(assert, false);
  },
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n  fn(value)\n'
  }
);

common.expectsError(
  () => assert.ok.call(null, 0),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'The expression evaluated to a falsy value:\n\n  ' +
             'assert.ok.call(null, 0)\n',
    generatedMessage: true
  }
);

common.expectsError(
  () => assert.ok.call(null, 0, 'test'),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'test',
    generatedMessage: false
  }
);

// Works in eval.
common.expectsError(
  () => new Function('assert', 'assert(1 === 2);')(assert),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'false == true'
  }
);

common.expectsError(
  () => assert.throws(() => {}, 'Error message', 'message'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "error" argument must be one of type Object, Error, ' +
             'Function, or RegExp. Received type string'
  }
);

[
  1,
  false,
  Symbol()
].forEach((input) => {
  assert.throws(
    () => assert.throws(() => {}, input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "error" argument must be one of type Object, Error, ' +
               `Function, or RegExp. Received type ${typeof input}`
    }
  );
});

{

  assert.throws(() => {
    assert.ok((() => Boolean('' === false))());
  }, {
    message: 'The expression evaluated to a falsy value:\n\n' +
             "  assert.ok((() => Boolean('\\u0001' === false))())\n"
  });

  const errFn = () => {
    const err = new TypeError('Wrong value');
    err.code = 404;
    throw err;
  };
  const errObj = {
    name: 'TypeError',
    message: 'Wrong value'
  };
  assert.throws(errFn, errObj);

  errObj.code = 404;
  assert.throws(errFn, errObj);

  // Fail in case a expected property is undefined and not existent on the
  // error.
  errObj.foo = undefined;
  assert.throws(
    () => assert.throws(errFn, errObj),
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError [ERR_ASSERTION]',
      message: `${start}\n${actExp}\n\n` +
               '  Comparison {\n' +
               '    code: 404,\n' +
               '-   foo: undefined,\n' +
               "    message: 'Wrong value',\n" +
               "    name: 'TypeError'\n" +
               '  }'
    }
  );

  // Show multiple wrong properties at the same time.
  errObj.code = '404';
  assert.throws(
    () => assert.throws(errFn, errObj),
    {
      code: 'ERR_ASSERTION',
      name: 'AssertionError [ERR_ASSERTION]',
      message: `${start}\n${actExp}\n\n` +
               '  Comparison {\n' +
               '+   code: 404,\n' +
               "-   code: '404',\n" +
               '-   foo: undefined,\n' +
               "    message: 'Wrong value',\n" +
               "    name: 'TypeError'\n" +
               '  }'
    }
  );

  common.expectsError(
    () => assert.throws(() => { throw new Error(); }, { foo: 'bar' }, 'foobar'),
    {
      type: assert.AssertionError,
      code: 'ERR_ASSERTION',
      message: 'foobar'
    }
  );

  common.expectsError(
    () => a.doesNotThrow(() => { throw new Error(); }, { foo: 'bar' }),
    {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "expected" argument must be one of type Function or ' +
               'RegExp. Received type object'
    }
  );

  assert.throws(() => { throw new Error('e'); }, new Error('e'));
  assert.throws(
    () => assert.throws(() => { throw new TypeError('e'); }, new Error('e')),
    {
      name: 'AssertionError [ERR_ASSERTION]',
      code: 'ERR_ASSERTION',
      message: `${start}\n${actExp}\n\n` +
               '  Comparison {\n' +
               "    message: 'e',\n" +
               "+   name: 'TypeError'\n" +
               "-   name: 'Error'\n" +
               '  }'
    }
  );
  assert.throws(
    () => assert.throws(() => { throw new Error('foo'); }, new Error('')),
    {
      name: 'AssertionError [ERR_ASSERTION]',
      code: 'ERR_ASSERTION',
      generatedMessage: true,
      message: `${start}\n${actExp}\n\n` +
               '  Comparison {\n' +
               "+   message: 'foo',\n" +
               "-   message: '',\n" +
               "    name: 'Error'\n" +
               '  }'
    }
  );

  // eslint-disable-next-line no-throw-literal
  assert.throws(() => { throw undefined; }, /undefined/);
  assert.throws(
    // eslint-disable-next-line no-throw-literal
    () => a.doesNotThrow(() => { throw undefined; }),
    {
      name: 'AssertionError [ERR_ASSERTION]',
      code: 'ERR_ASSERTION',
      message: 'Got unwanted exception.\nActual message: "undefined"'
    }
  );
}

assert.throws(
  () => assert.throws(() => { throw new Error(); }, {}),
  {
    message: "The argument 'error' may not be an empty object. Received {}",
    code: 'ERR_INVALID_ARG_VALUE'
  }
);

assert.throws(
  () => a.throws(
    // eslint-disable-next-line no-throw-literal
    () => { throw 'foo'; },
    'foo'
  ),
  {
    code: 'ERR_AMBIGUOUS_ARGUMENT',
    message: 'The "error/message" argument is ambiguous. ' +
             'The error "foo" is identical to the message.'
  }
);

assert.throws(
  () => a.throws(
    () => { throw new TypeError('foo'); },
    'foo'
  ),
  {
    code: 'ERR_AMBIGUOUS_ARGUMENT',
    message: 'The "error/message" argument is ambiguous. ' +
             'The error message "foo" is identical to the message.'
  }
);
/* eslint-enable no-restricted-syntax */

// Should not throw.
// eslint-disable-next-line no-restricted-syntax, no-throw-literal
assert.throws(() => { throw null; }, 'foo');

assert.throws(
  () => assert.strictEqual([], []),
  {
    message: 'Values identical but not reference-equal:\n\n[]\n'
  }
);

{
  const args = (function() { return arguments; })('a');
  assert.throws(
    () => assert.strictEqual(args, { 0: 'a' }),
    {
      message: 'Expected "actual" to be reference-equal to "expected":\n' +
               '+ actual - expected\n\n' +
               "+ [Arguments] {\n- {\n    '0': 'a'\n  }"
    }
  );
}

assert.throws(
  () => { throw new TypeError('foobar'); },
  {
    message: /foo/,
    name: /^TypeError$/
  }
);

assert.throws(
  () => assert.throws(
    () => { throw new TypeError('foobar'); },
    {
      message: /fooa/,
      name: /^TypeError$/
    }
  ),
  {
    message: `${start}\n${actExp}\n\n` +
             '  Comparison {\n' +
             "+   message: 'foobar',\n" +
             '-   message: /fooa/,\n' +
             "    name: 'TypeError'\n" +
             '  }'
  }
);

{
  let actual = null;
  const expected = { message: 'foo' };
  assert.throws(
    () => assert.throws(
      () => { throw actual; },
      expected
    ),
    {
      operator: 'throws',
      actual,
      expected,
      generatedMessage: true,
      message: `${start}\n${actExp}\n\n` +
              '+ null\n' +
              '- {\n' +
              "-   message: 'foo'\n" +
              '- }'
    }
  );

  actual = 'foobar';
  const message = 'message';
  assert.throws(
    () => assert.throws(
      () => { throw actual; },
      { message: 'foobar' },
      message
    ),
    {
      actual,
      message,
      operator: 'throws',
      generatedMessage: false
    }
  );
}

// Indicate where the strings diverge.
assert.throws(
  () => assert.strictEqual('test test', 'test foobar'),
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError [ERR_ASSERTION]',
    message: strictEqualMessageStart +
             '+ actual - expected\n\n' +
             "+ 'test test'\n" +
             "- 'test foobar'\n" +
             '        ^'
  }
);

// Check for reference-equal objects in `notStrictEqual()`
assert.throws(
  () => {
    const obj = {};
    assert.notStrictEqual(obj, obj);
  },
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError [ERR_ASSERTION]',
    message: 'Expected "actual" not to be reference-equal to "expected": {}'
  }
);

assert.throws(
  () => {
    const obj = { a: true };
    assert.notStrictEqual(obj, obj);
  },
  {
    code: 'ERR_ASSERTION',
    name: 'AssertionError [ERR_ASSERTION]',
    message: 'Expected "actual" not to be reference-equal to "expected":\n\n' +
             '{\n  a: true\n}\n'
  }
);

{
  let threw = false;
  try {
    assert.deepStrictEqual(Array(100).fill(1), 'foobar');
  } catch (err) {
    threw = true;
    assert(/actual: \[Array],\n  expected: 'foobar',/.test(inspect(err)));
  }
  assert(threw);
}
