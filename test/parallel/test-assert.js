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
const { EOL } = require('os');
const a = assert;

function makeBlock(f) {
  const args = Array.prototype.slice.call(arguments, 1);
  return () => {
    return f.apply(null, args);
  };
}

assert.ok(a.AssertionError.prototype instanceof Error,
          'a.AssertionError instanceof Error');

assert.throws(makeBlock(a, false), a.AssertionError, 'ok(false)');

assert.doesNotThrow(makeBlock(a, true), a.AssertionError, 'ok(true)');

assert.doesNotThrow(makeBlock(a, 'test', 'ok(\'test\')'));

assert.throws(makeBlock(a.ok, false),
              a.AssertionError, 'ok(false)');

assert.doesNotThrow(makeBlock(a.ok, true),
                    a.AssertionError, 'ok(true)');

assert.doesNotThrow(makeBlock(a.ok, 'test'), 'ok(\'test\')');

assert.throws(makeBlock(a.equal, true, false),
              a.AssertionError, 'equal(true, false)');

assert.doesNotThrow(makeBlock(a.equal, null, null),
                    'equal(null, null)');

assert.doesNotThrow(makeBlock(a.equal, undefined, undefined),
                    'equal(undefined, undefined)');

assert.doesNotThrow(makeBlock(a.equal, null, undefined),
                    'equal(null, undefined)');

assert.doesNotThrow(makeBlock(a.equal, true, true), 'equal(true, true)');

assert.doesNotThrow(makeBlock(a.equal, 2, '2'), 'equal(2, \'2\')');

assert.doesNotThrow(makeBlock(a.notEqual, true, false),
                    'notEqual(true, false)');

assert.throws(makeBlock(a.notEqual, true, true),
              a.AssertionError, 'notEqual(true, true)');

assert.throws(makeBlock(a.strictEqual, 2, '2'),
              a.AssertionError, 'strictEqual(2, \'2\')');

assert.throws(makeBlock(a.strictEqual, null, undefined),
              a.AssertionError, 'strictEqual(null, undefined)');

assert.throws(makeBlock(a.notStrictEqual, 2, 2),
              a.AssertionError, 'notStrictEqual(2, 2)');

assert.doesNotThrow(makeBlock(a.notStrictEqual, 2, '2'),
                    'notStrictEqual(2, \'2\')');

// deepEqual joy!
assert.doesNotThrow(makeBlock(a.deepEqual, new Date(2000, 3, 14),
                              new Date(2000, 3, 14)),
                    'deepEqual(new Date(2000, 3, 14), new Date(2000, 3, 14))');

assert.throws(makeBlock(a.deepEqual, new Date(), new Date(2000, 3, 14)),
              a.AssertionError,
              'deepEqual(new Date(), new Date(2000, 3, 14))');

assert.throws(
  makeBlock(a.notDeepEqual, new Date(2000, 3, 14), new Date(2000, 3, 14)),
  a.AssertionError,
  'notDeepEqual(new Date(2000, 3, 14), new Date(2000, 3, 14))'
);

assert.doesNotThrow(makeBlock(
  a.notDeepEqual,
  new Date(),
  new Date(2000, 3, 14)),
                    'notDeepEqual(new Date(), new Date(2000, 3, 14))'
);

assert.doesNotThrow(makeBlock(a.deepEqual, /a/, /a/));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/g, /a/g));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/i, /a/i));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/m, /a/m));
assert.doesNotThrow(makeBlock(a.deepEqual, /a/igm, /a/igm));
assert.throws(makeBlock(a.deepEqual, /ab/, /a/),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\/ab\/ deepEqual \/a\/$/
              }));
assert.throws(makeBlock(a.deepEqual, /a/g, /a/),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\/a\/g deepEqual \/a\/$/
              }));
assert.throws(makeBlock(a.deepEqual, /a/i, /a/),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\/a\/i deepEqual \/a\/$/
              }));
assert.throws(makeBlock(a.deepEqual, /a/m, /a/),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\/a\/m deepEqual \/a\/$/
              }));
assert.throws(makeBlock(a.deepEqual, /a/igm, /a/im),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\/a\/gim deepEqual \/a\/im$/
              }));

{
  const re1 = /a/g;
  re1.lastIndex = 3;
  assert.doesNotThrow(makeBlock(a.deepEqual, re1, /a/g));
}

assert.doesNotThrow(makeBlock(a.deepEqual, 4, '4'), 'deepEqual(4, \'4\')');
assert.doesNotThrow(makeBlock(a.deepEqual, true, 1), 'deepEqual(true, 1)');
assert.throws(makeBlock(a.deepEqual, 4, '5'),
              a.AssertionError,
              'deepEqual( 4, \'5\')');

// having the same number of owned properties && the same set of keys
assert.doesNotThrow(makeBlock(a.deepEqual, { a: 4 }, { a: 4 }));
assert.doesNotThrow(makeBlock(a.deepEqual, { a: 4, b: '2' }, { a: 4, b: '2' }));
assert.doesNotThrow(makeBlock(a.deepEqual, [4], ['4']));
assert.throws(makeBlock(a.deepEqual, { a: 4 }, { a: 4, b: true }),
              a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, ['a'], { 0: 'a' }));
//(although not necessarily the same order),
assert.doesNotThrow(makeBlock(a.deepEqual, { a: 4, b: '1' }, { b: '1', a: 4 }));
const a1 = [1, 2, 3];
const a2 = [1, 2, 3];
a1.a = 'test';
a1.b = true;
a2.b = true;
a2.a = 'test';
assert.throws(makeBlock(a.deepEqual, Object.keys(a1), Object.keys(a2)),
              a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, a1, a2));

// having an identical prototype property
const nbRoot = {
  toString() { return `${this.first} ${this.last}`; }
};

function nameBuilder(first, last) {
  this.first = first;
  this.last = last;
  return this;
}
nameBuilder.prototype = nbRoot;

function nameBuilder2(first, last) {
  this.first = first;
  this.last = last;
  return this;
}
nameBuilder2.prototype = nbRoot;

const nb1 = new nameBuilder('Ryan', 'Dahl');
let nb2 = new nameBuilder2('Ryan', 'Dahl');

assert.doesNotThrow(makeBlock(a.deepEqual, nb1, nb2));

nameBuilder2.prototype = Object;
nb2 = new nameBuilder2('Ryan', 'Dahl');
assert.doesNotThrow(makeBlock(a.deepEqual, nb1, nb2));

// primitives and object
assert.throws(makeBlock(a.deepEqual, null, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, undefined, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, 'a', ['a']), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, 'a', { 0: 'a' }), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, 1, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, true, {}), a.AssertionError);
assert.throws(makeBlock(a.deepEqual, Symbol(), {}), a.AssertionError);

// primitive wrappers and object
assert.doesNotThrow(makeBlock(a.deepEqual, new String('a'), ['a']),
                    a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, new String('a'), { 0: 'a' }),
                    a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, new Number(1), {}),
                    a.AssertionError);
assert.doesNotThrow(makeBlock(a.deepEqual, new Boolean(true), {}),
                    a.AssertionError);

// same number of keys but different key names
assert.throws(makeBlock(a.deepEqual, { a: 1 }, { b: 1 }), a.AssertionError);

//deepStrictEqual
assert.doesNotThrow(
  makeBlock(a.deepStrictEqual, new Date(2000, 3, 14), new Date(2000, 3, 14)),
  'deepStrictEqual(new Date(2000, 3, 14), new Date(2000, 3, 14))'
);

assert.throws(
  makeBlock(a.deepStrictEqual, new Date(), new Date(2000, 3, 14)),
  a.AssertionError,
  'deepStrictEqual(new Date(), new Date(2000, 3, 14))'
);

assert.throws(
  makeBlock(a.notDeepStrictEqual, new Date(2000, 3, 14), new Date(2000, 3, 14)),
  a.AssertionError,
  'notDeepStrictEqual(new Date(2000, 3, 14), new Date(2000, 3, 14))'
);

assert.doesNotThrow(
  makeBlock(a.notDeepStrictEqual, new Date(), new Date(2000, 3, 14)),
  'notDeepStrictEqual(new Date(), new Date(2000, 3, 14))'
);

assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/, /a/));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/g, /a/g));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/i, /a/i));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/m, /a/m));
assert.doesNotThrow(makeBlock(a.deepStrictEqual, /a/igm, /a/igm));
assert.throws(
  makeBlock(a.deepStrictEqual, /ab/, /a/),
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: a.AssertionError,
    message: /^\/ab\/ deepStrictEqual \/a\/$/
  }));
assert.throws(
  makeBlock(a.deepStrictEqual, /a/g, /a/),
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: a.AssertionError,
    message: /^\/a\/g deepStrictEqual \/a\/$/
  }));
assert.throws(
  makeBlock(a.deepStrictEqual, /a/i, /a/),
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: a.AssertionError,
    message: /^\/a\/i deepStrictEqual \/a\/$/
  }));
assert.throws(
  makeBlock(a.deepStrictEqual, /a/m, /a/),
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: a.AssertionError,
    message: /^\/a\/m deepStrictEqual \/a\/$/
  }));
assert.throws(
  makeBlock(a.deepStrictEqual, /a/igm, /a/im),
  common.expectsError({
    code: 'ERR_ASSERTION',
    type: a.AssertionError,
    message: /^\/a\/gim deepStrictEqual \/a\/im$/
  }));

{
  const re1 = /a/;
  re1.lastIndex = 3;
  assert.doesNotThrow(makeBlock(a.deepStrictEqual, re1, /a/));
}

assert.throws(makeBlock(a.deepStrictEqual, 4, '4'),
              a.AssertionError,
              'deepStrictEqual(4, \'4\')');

assert.throws(makeBlock(a.deepStrictEqual, true, 1),
              a.AssertionError,
              'deepStrictEqual(true, 1)');

assert.throws(makeBlock(a.deepStrictEqual, 4, '5'),
              a.AssertionError,
              'deepStrictEqual(4, \'5\')');

// having the same number of owned properties && the same set of keys
assert.doesNotThrow(makeBlock(a.deepStrictEqual, { a: 4 }, { a: 4 }));
assert.doesNotThrow(makeBlock(a.deepStrictEqual,
                              { a: 4, b: '2' },
                              { a: 4, b: '2' }));
assert.throws(makeBlock(a.deepStrictEqual, [4], ['4']),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\[ 4 ] deepStrictEqual \[ '4' ]$/
              }));
assert.throws(makeBlock(a.deepStrictEqual, { a: 4 }, { a: 4, b: true }),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^{ a: 4 } deepStrictEqual { a: 4, b: true }$/
              }));
assert.throws(makeBlock(a.deepStrictEqual, ['a'], { 0: 'a' }),
              common.expectsError({
                code: 'ERR_ASSERTION',
                type: a.AssertionError,
                message: /^\[ 'a' ] deepStrictEqual { '0': 'a' }$/
              }));
//(although not necessarily the same order),
assert.doesNotThrow(makeBlock(a.deepStrictEqual,
                              { a: 4, b: '1' },
                              { b: '1', a: 4 }));

assert.throws(makeBlock(a.deepStrictEqual,
                        [0, 1, 2, 'a', 'b'],
                        [0, 1, 2, 'b', 'a']),
              a.AssertionError);

assert.doesNotThrow(makeBlock(a.deepStrictEqual, a1, a2));

// Prototype check
function Constructor1(first, last) {
  this.first = first;
  this.last = last;
}

function Constructor2(first, last) {
  this.first = first;
  this.last = last;
}

const obj1 = new Constructor1('Ryan', 'Dahl');
let obj2 = new Constructor2('Ryan', 'Dahl');

assert.throws(makeBlock(a.deepStrictEqual, obj1, obj2), a.AssertionError);

Constructor2.prototype = Constructor1.prototype;
obj2 = new Constructor2('Ryan', 'Dahl');

assert.doesNotThrow(makeBlock(a.deepStrictEqual, obj1, obj2));

// primitives
assert.throws(makeBlock(assert.deepStrictEqual, 4, '4'),
              a.AssertionError);
assert.throws(makeBlock(assert.deepStrictEqual, true, 1),
              a.AssertionError);
assert.throws(makeBlock(assert.deepStrictEqual, Symbol(), Symbol()),
              a.AssertionError);

const s = Symbol();
assert.doesNotThrow(makeBlock(assert.deepStrictEqual, s, s));


// primitives and object
assert.throws(makeBlock(a.deepStrictEqual, null, {}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, undefined, {}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, 'a', ['a']), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, 'a', { 0: 'a' }), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, 1, {}), a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, true, {}), a.AssertionError);
assert.throws(makeBlock(assert.deepStrictEqual, Symbol(), {}),
              a.AssertionError);


// primitive wrappers and object
assert.throws(makeBlock(a.deepStrictEqual, new String('a'), ['a']),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, new String('a'), { 0: 'a' }),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, new Number(1), {}),
              a.AssertionError);
assert.throws(makeBlock(a.deepStrictEqual, new Boolean(true), {}),
              a.AssertionError);


// Testing the throwing
function thrower(errorConstructor) {
  throw new errorConstructor({});
}

// the basic calls work
assert.throws(makeBlock(thrower, a.AssertionError),
              a.AssertionError, 'message');
assert.throws(makeBlock(thrower, a.AssertionError), a.AssertionError);
// eslint-disable-next-line no-restricted-syntax
assert.throws(makeBlock(thrower, a.AssertionError));

// if not passing an error, catch all.
// eslint-disable-next-line no-restricted-syntax
assert.throws(makeBlock(thrower, TypeError));

// when passing a type, only catch errors of the appropriate type
{
  let threw = false;
  try {
    a.throws(makeBlock(thrower, TypeError), a.AssertionError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof TypeError, 'type');
  }
  assert.strictEqual(true, threw,
                     'a.throws with an explicit error is eating extra errors');
}

// doesNotThrow should pass through all errors
{
  let threw = false;
  try {
    a.doesNotThrow(makeBlock(thrower, TypeError), a.AssertionError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof TypeError);
  }
  assert.strictEqual(true, threw, 'a.doesNotThrow with an explicit error is ' +
                     'eating extra errors');
}

// key difference is that throwing our correct error makes an assertion error
{
  let threw = false;
  try {
    a.doesNotThrow(makeBlock(thrower, TypeError), TypeError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof a.AssertionError);
  }
  assert.strictEqual(true, threw,
                     'a.doesNotThrow is not catching type matching errors');
}

common.expectsError(
  () => assert.doesNotThrow(makeBlock(thrower, Error), 'user message'),
  {
    type: a.AssertionError,
    code: 'ERR_ASSERTION',
    operator: 'doesNotThrow',
    message: 'Got unwanted exception: user message\n[object Object]'
  }
);

common.expectsError(
  () => assert.doesNotThrow(makeBlock(thrower, Error), 'user message'),
  {
    code: 'ERR_ASSERTION',
    message: /Got unwanted exception: user message\n\[object Object\]/
  }
);

common.expectsError(
  () => assert.doesNotThrow(makeBlock(thrower, Error)),
  {
    code: 'ERR_ASSERTION',
    message: /Got unwanted exception\.\n\[object Object\]/
  }
);

// make sure that validating using constructor really works
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

// use a RegExp to validate error message
a.throws(makeBlock(thrower, TypeError), /\[object Object\]/);

// use a fn to validate error object
a.throws(makeBlock(thrower, TypeError), (err) => {
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

    const functionThatThrows = () => {
      throw new AnotherErrorType('foo');
    };

    assert.throws(functionThatThrows, ES6Error);
  } catch (e) {
    threw = true;
    assert(e instanceof AnotherErrorType,
           `expected AnotherErrorType, received ${e}`);
  }

  assert.ok(threw);
}

// check messages from assert.throws()
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
                       [expected, 'strictEqual', '\'\''].join(' '));
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

// https://github.com/nodejs/node-v0.x-archive/issues/5292
try {
  assert.strictEqual(1, 2);
} catch (e) {
  assert.strictEqual(e.message.split('\n')[0], '1 strictEqual 2');
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

  // verify custom errors
  try {
    assert.strictEqual(1, 2, rangeError);
  } catch (e) {
    assert.strictEqual(e, rangeError);
    threw = true;
    assert.ok(e instanceof RangeError, 'Incorrect error type thrown');
  }
  assert.ok(threw);
  threw = false;

  // verify AssertionError is the result from doesNotThrow with custom Error
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
  // Verify that throws() and doesNotThrow() throw on non-function block
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
  message: /^'A{124}\.\.\. strictEqual ''$/
}));

{
  // bad args to AssertionError constructor should throw TypeError
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
    message: /^'Error: foo' strictEqual 'Error: foobar'$/
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
      code: 'ERR_MISSING_ARGS',
      type: TypeError
    }
  );
  common.expectsError(
    () => a(),
    {
      code: 'ERR_MISSING_ARGS',
      type: TypeError
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
      message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
               `assert.ok(typeof 123 === 'string')${EOL}`
    }
  );
  Error.stackTraceLimit = tmpLimit;

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
    message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
             `assert.ok(null)${EOL}`
  }
);
common.expectsError(
  () => assert(typeof 123 === 'string'),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
             `assert(typeof 123 === 'string')${EOL}`
  }
);

{
  // Test caching
  const fs = process.binding('fs');
  const tmp = fs.close;
  fs.close = common.mustCall(tmp, 1);
  function throwErr() {
    // eslint-disable-next-line prefer-assert-methods
    assert(
      (Buffer.from('test') instanceof Error)
    );
  }
  common.expectsError(
    () => throwErr(),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
               `assert(Buffer.from('test') instanceof Error)${EOL}`
    }
  );
  common.expectsError(
    () => throwErr(),
    {
      code: 'ERR_ASSERTION',
      type: assert.AssertionError,
      message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
               `assert(Buffer.from('test') instanceof Error)${EOL}`
    }
  );
  fs.close = tmp;
}

common.expectsError(
  () => {
    a(
      (() => 'string')()
      // eslint-disable-next-line
      ===
      123 instanceof
          Buffer
    );
  },
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
             `assert((() => 'string')()${EOL}` +
             `      // eslint-disable-next-line${EOL}` +
             `      ===${EOL}` +
             `      123 instanceof${EOL}` +
             `          Buffer)${EOL}`
  }
);

common.expectsError(
  () => assert(null, undefined),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: `The expression evaluated to a falsy value:${EOL}${EOL}  ` +
             `assert(null, undefined)${EOL}`
  }
);

common.expectsError(
  () => assert.ok.apply(null, [0]),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: '0 == true'
  }
);

common.expectsError(
  () => assert.ok.call(null, 0),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: '0 == true'
  }
);

common.expectsError(
  () => assert.ok.call(null, 0, 'test'),
  {
    code: 'ERR_ASSERTION',
    type: assert.AssertionError,
    message: 'test'
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
