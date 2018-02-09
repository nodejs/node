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

/* eslint-disable prefer-common-expectserror */

const common = require('../common');
const assert = require('assert');
const { inspect } = require('util');
const a = assert;

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

assert.throws(() => a.strictEqual(null, undefined),
              a.AssertionError, 'strictEqual(null, undefined)');

assert.throws(() => a.notStrictEqual(2, 2),
              a.AssertionError, 'notStrictEqual(2, 2)');

a.notStrictEqual(2, '2');

// Testing the throwing.
function thrower(errorConstructor) {
  throw new errorConstructor({});
}

// The basic calls work.
assert.throws(() => thrower(a.AssertionError), a.AssertionError, 'message');
assert.throws(() => thrower(a.AssertionError), a.AssertionError);
// eslint-disable-next-line no-restricted-syntax
assert.throws(() => thrower(a.AssertionError));

// If not passing an error, catch all.
// eslint-disable-next-line no-restricted-syntax
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
  }
  assert.ok(threw, 'a.doesNotThrow is not catching type matching errors');
}

assert.throws(() => { assert.ifError(new Error('test error')); },
              /^Error: test error$/);
assert.ifError(null);
assert.ifError();

common.expectsError(
  () => assert.doesNotThrow(() => thrower(Error), 'user message'),
  {
    type: a.AssertionError,
    code: 'ERR_ASSERTION',
    operator: 'doesNotThrow',
    message: 'Got unwanted exception: user message\n[object Object]'
  }
);

common.expectsError(
  () => assert.doesNotThrow(() => thrower(Error), 'user message'),
  {
    code: 'ERR_ASSERTION',
    message: /Got unwanted exception: user message\n\[object Object\]/
  }
);

common.expectsError(
  () => assert.doesNotThrow(() => thrower(Error)),
  {
    code: 'ERR_ASSERTION',
    message: /Got unwanted exception\.\n\[object Object\]/
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
  } catch (e) {
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
    common.expectsError({
      code: 'ERR_ASSERTION',
      message: /^Missing expected exception\.$/,
      operator: 'throws'
    }));

  assert.throws(
    () => { a.throws(noop, TypeError); },
    common.expectsError({
      code: 'ERR_ASSERTION',
      message: /^Missing expected exception \(TypeError\)\.$/
    }));

  assert.throws(
    () => { a.throws(noop, 'fhqwhgads'); },
    common.expectsError({
      code: 'ERR_ASSERTION',
      message: /^Missing expected exception: fhqwhgads$/
    }));

  assert.throws(
    () => { a.throws(noop, TypeError, 'fhqwhgads'); },
    common.expectsError({
      code: 'ERR_ASSERTION',
      message: /^Missing expected exception \(TypeError\): fhqwhgads$/
    }));
}

const circular = { y: 1 };
circular.x = circular;

function testAssertionMessage(actual, expected) {
  try {
    assert.strictEqual(actual, '');
  } catch (e) {
    assert.strictEqual(e.message,
                       [expected, '===', '\'\''].join(' '));
    assert.ok(e.generatedMessage, 'Message not marked as generated');
  }
}

testAssertionMessage(undefined, 'undefined');
testAssertionMessage(null, 'null');
testAssertionMessage(true, 'true');
testAssertionMessage(false, 'false');
testAssertionMessage(0, '0');
testAssertionMessage(100, '100');
testAssertionMessage(NaN, 'NaN');
testAssertionMessage(Infinity, 'Infinity');
testAssertionMessage(-Infinity, '-Infinity');
testAssertionMessage('', '""');
testAssertionMessage('foo', '\'foo\'');
testAssertionMessage([], '[]');
testAssertionMessage([1, 2, 3], '[ 1, 2, 3 ]');
testAssertionMessage(/a/, '/a/');
testAssertionMessage(/abc/gim, '/abc/gim');
testAssertionMessage(function f() {}, '[Function: f]');
testAssertionMessage(function() {}, '[Function]');
testAssertionMessage({}, '{}');
testAssertionMessage(circular, '{ y: 1, x: [Circular] }');
testAssertionMessage({ a: undefined, b: null }, '{ a: undefined, b: null }');
testAssertionMessage({ a: NaN, b: Infinity, c: -Infinity },
                     '{ a: NaN, b: Infinity, c: -Infinity }');

// https://github.com/nodejs/node-v0.x-archive/issues/2893
{
  let threw = false;
  try {
    // eslint-disable-next-line no-restricted-syntax
    assert.throws(() => {
      assert.ifError(null);
    });
  } catch (e) {
    threw = true;
    assert.strictEqual(e.message, 'Missing expected exception.');
    assert.ok(!e.stack.includes('throws'), e.stack);
  }
  assert.ok(threw);
}

// https://github.com/nodejs/node-v0.x-archive/issues/5292
try {
  assert.strictEqual(1, 2);
} catch (e) {
  assert.strictEqual(e.message.split('\n')[0], '1 === 2');
  assert.ok(e.generatedMessage, 'Message not marked as generated');
}

try {
  assert.strictEqual(1, 2, 'oh no');
} catch (e) {
  assert.strictEqual(e.message.split('\n')[0], 'oh no');
  assert.strictEqual(e.generatedMessage, false,
                     'Message incorrectly marked as generated');
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
    assert.doesNotThrow(() => {
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
  // Verify that throws() and doesNotThrow() throw on non-function block.
  function typeName(value) {
    return value === null ? 'null' : typeof value;
  }

  const testBlockTypeError = (method, block) => {
    common.expectsError(
      () => method(block),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "block" argument must be of type Function. Received ' +
                `type ${typeName(block)}`
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
}, common.expectsError({
  code: 'ERR_ASSERTION',
  message: /^'A{124}\.\.\. === ''$/
}));

{
  // Bad args to AssertionError constructor should throw TypeError.
  const args = [1, true, false, '', null, Infinity, Symbol('test'), undefined];
  const re = /^The "options" argument must be of type Object$/;
  args.forEach((input) => {
    assert.throws(
      () => new assert.AssertionError(input),
      common.expectsError({
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: re
      }));
  });
}

common.expectsError(
  () => assert.strictEqual(new Error('foo'), new Error('foobar')),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: /^'Error: foo' === 'Error: foobar'$/
  }
);

// Test strict assert
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
  common.expectsError(
    () => assert(),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'undefined == true'
    }
  );

  // Test error diffs
  const colors = process.stdout.isTTY && process.stdout.getColorDepth() > 1;
  const start = 'Input A expected to deepStrictEqual input B:';
  const actExp = colors ?
    '\u001b[32m+ expected\u001b[39m \u001b[31m- actual\u001b[39m' :
    '+ expected - actual';
  const plus = colors ? '\u001b[32m+\u001b[39m' : '+';
  const minus = colors ? '\u001b[31m-\u001b[39m' : '-';
  let message = [
    start,
    `${actExp} ... Lines skipped`,
    '',
    '  [',
    '    [',
    '...',
    '        2,',
    `${minus}       3`,
    `${plus}       '3'`,
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
    `${plus}   1,`,
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
    `${minus}   1,`,
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
    `${minus}   2,`,
    `${plus}   1,`,
    '    1,',
    '    1,',
    '    0,',
    `${minus}   1,`,
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
    `${minus} [`,
    `${minus}   1,`,
    `${minus}   2,`,
    `${minus}   1`,
    `${minus} ]`,
    `${plus} undefined`,
  ].join('\n');
  assert.throws(
    () => assert.deepEqual([1, 2, 1]),
    { message });

  message = [
    start,
    actExp,
    '',
    '  [',
    `${minus}   1,`,
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
    `${minus}   1,\n`.repeat(10) +
    '...\n' +
    `${plus}   2,\n`.repeat(10) +
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
    `${minus} {}\n` +
    `${plus} {\n` +
    `${plus}   loop: 'forever',\n` +
    `${plus}   [Symbol(util.inspect.custom)]: [Function]\n` +
    `${plus} }`
  });

  // notDeepEqual tests
  message = 'Identical input passed to notDeepStrictEqual:\n[\n  1\n]';
  assert.throws(
    () => assert.notDeepEqual([1], [1]),
    { message });

  message = 'Identical input passed to notDeepStrictEqual:' +
        `\n[${'\n  1,'.repeat(18)}\n...`;
  const data = Array(21).fill(1);
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
    message: 'null == true'
  }
);

common.expectsError(
  // eslint-disable-next-line no-restricted-syntax
  () => assert.throws(() => {}, 'Error message', 'message'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "error" argument must be one of type Function or RegExp. ' +
             'Received type string'
  }
);

{
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

  errObj.code = '404';
  common.expectsError(
  // eslint-disable-next-line no-restricted-syntax
    () => assert.throws(errFn, errObj),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'code: expected \'404\', not 404'
    }
  );

  errObj.code = 404;
  errObj.foo = 'bar';
  common.expectsError(
  // eslint-disable-next-line no-restricted-syntax
    () => assert.throws(errFn, errObj),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: 'foo: expected \'bar\', not undefined'
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
    () => assert.doesNotThrow(() => { throw new Error(); }, { foo: 'bar' }),
    {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "expected" argument must be one of type Function or ' +
               'RegExp. Received type object'
    }
  );

  assert.throws(() => { throw new Error('e'); }, new Error('e'));
  common.expectsError(
    () => assert.throws(() => { throw new TypeError('e'); }, new Error('e')),
    {
      type: assert.AssertionError,
      code: 'ERR_ASSERTION',
      message: "name: expected 'Error', not 'TypeError'"
    }
  );
  common.expectsError(
    () => assert.throws(() => { throw new Error('foo'); }, new Error('')),
    {
      type: assert.AssertionError,
      code: 'ERR_ASSERTION',
      message: "message: expected '', not 'foo'"
    }
  );

  // eslint-disable-next-line no-throw-literal
  assert.throws(() => { throw undefined; }, /undefined/);
  common.expectsError(
    // eslint-disable-next-line no-throw-literal
    () => assert.doesNotThrow(() => { throw undefined; }),
    {
      type: assert.AssertionError,
      code: 'ERR_ASSERTION',
      message: 'Got unwanted exception.\nundefined'
    }
  );
}
