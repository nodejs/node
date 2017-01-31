// Originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the 'Software'), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

// UTILITY
const compare = process.binding('buffer').compare;
const util = require('util');
const objectToString = require('internal/util').objectToString;
const Buffer = require('buffer').Buffer;

// The assert module provides functions that throw
// AssertionError's when particular conditions are not met. The
// assert module must conform to the following interface.

const assert = module.exports = ok;

// The AssertionError is defined in assert.
// new assert.AssertionError({ message: message,
//                             actual: actual,
//                             expected: expected });

assert.AssertionError = function AssertionError(options) {
  this.name = 'AssertionError';
  this.actual = options.actual;
  this.expected = options.expected;
  this.operator = options.operator;
  if (options.message) {
    this.message = options.message;
    this.generatedMessage = false;
  } else {
    this.message = getMessage(this);
    this.generatedMessage = true;
  }
  var stackStartFunction = options.stackStartFunction || fail;
  Error.captureStackTrace(this, stackStartFunction);
};

// assert.AssertionError instanceof Error
util.inherits(assert.AssertionError, Error);

function truncate(s, n) {
  return s.slice(0, n);
}

function getMessage(self) {
  return truncate(util.inspect(self.actual), 128) + ' ' +
         self.operator + ' ' +
         truncate(util.inspect(self.expected), 128);
}

// At present only the three keys mentioned above are used and
// understood by the spec. Implementations or sub modules can pass
// other keys to the AssertionError's constructor - they will be
// ignored.

// All of the following functions must throw an AssertionError
// when a corresponding condition is not met, with a message that
// may be undefined if not provided.  All assertion methods provide
// both the actual and expected values to the assertion error for
// display purposes.

function fail(actual, expected, message, operator, stackStartFunction) {
  throw new assert.AssertionError({
    message: message,
    actual: actual,
    expected: expected,
    operator: operator,
    stackStartFunction: stackStartFunction
  });
}

// EXTENSION! allows for well behaved errors defined elsewhere.
assert.fail = fail;

// Pure assertion tests whether a value is truthy, as determined
// by !!guard.
// assert.ok(guard, message_opt);
// This statement is equivalent to assert.equal(true, !!guard,
// message_opt);. To test strictly for the value true, use
// assert.strictEqual(true, guard, message_opt);.

function ok(value, message) {
  if (!value) fail(value, true, message, '==', assert.ok);
}
assert.ok = ok;

// The equality assertion tests shallow, coercive equality with
// ==.
// assert.equal(actual, expected, message_opt);
/* eslint-disable no-restricted-properties */
assert.equal = function equal(actual, expected, message) {
  if (actual != expected) fail(actual, expected, message, '==', assert.equal);
};

// The non-equality assertion tests for whether two objects are not
// equal with !=.
// assert.notEqual(actual, expected, message_opt);

assert.notEqual = function notEqual(actual, expected, message) {
  if (actual == expected) {
    fail(actual, expected, message, '!=', assert.notEqual);
  }
};

// The equivalence assertion tests a deep equality relation.
// assert.deepEqual(actual, expected, message_opt);

assert.deepEqual = function deepEqual(actual, expected, message) {
  if (!_deepEqual(actual, expected, false)) {
    fail(actual, expected, message, 'deepEqual', assert.deepEqual);
  }
};
/* eslint-enable */

assert.deepStrictEqual = function deepStrictEqual(actual, expected, message) {
  if (!_deepEqual(actual, expected, true)) {
    fail(actual, expected, message, 'deepStrictEqual', assert.deepStrictEqual);
  }
};

function areSimilarRegExps(a, b) {
  return a.source === b.source && a.flags === b.flags;
}

function areSimilarTypedArrays(a, b) {
  return compare(Buffer.from(a.buffer,
                             a.byteOffset,
                             a.byteLength),
                 Buffer.from(b.buffer,
                             b.byteOffset,
                             b.byteLength)) === 0;
}

function isNullOrNonObj(object) {
  return object === null || typeof object !== 'object';
}

function isFloatTypedArrayTag(tag) {
  return tag === '[object Float32Array]' || tag === '[object Float64Array]';
}

function isArguments(tag) {
  return tag === '[object Arguments]';
}

function _deepEqual(actual, expected, strict, memos) {
  // All identical values are equivalent, as determined by ===.
  if (actual === expected) {
    return true;
  }

  // For primitives / functions
  // (determined by typeof value !== 'object'),
  // or null, equivalence is determined by === or ==.
  if (isNullOrNonObj(actual) && isNullOrNonObj(expected)) {
    return strict ? actual === expected : actual == expected;
  }

  // If they bypass the previous check, then at least
  // one of them must be an non-null object.
  // If the other one is null or undefined, they must not be equal.
  if (actual === null || actual === undefined ||
      expected === null || expected === undefined)
    return false;

  // Notes: Type tags are historical [[Class]] properties that can be set by
  // FunctionTemplate::SetClassName() in C++ or Symbol.toStringTag in JS
  // and retrieved using Object.prototype.toString.call(obj) in JS
  // See https://tc39.github.io/ecma262/#sec-object.prototype.tostring
  // for a list of tags pre-defined in the spec.
  // There are some unspecified tags in the wild too (e.g. typed array tags).
  // Since tags can be altered, they only serve fast failures
  const actualTag = objectToString(actual);
  const expectedTag = objectToString(expected);

  // Passing null or undefined to Object.getPrototypeOf() will throw
  // so this must done after previous checks.
  // For strict comparison, objects should have
  // a) The same prototypes.
  // b) The same built-in type tags
  if (strict) {
    if (Object.getPrototypeOf(actual) !== Object.getPrototypeOf(expected)) {
      return false;
    }

    if (actualTag !== expectedTag) {
      return false;
    }
  }

  // Do fast checks for builtin types.
  // If they don't match, they must not be equal.
  // If they match, return true for non-strict comparison.
  // For strict comparison we need to exam further.

  // If both values are Date objects,
  // check if the time underneath are equal first.
  if (util.isDate(actual) && util.isDate(expected)) {
    if (actual.getTime() !== expected.getTime()) {
      return false;
    } else if (!strict) {
      return true;  // Skip further checks for non-strict comparison.
    }
  }

  // If both values are RegExp, check if they have
  // the same source and flags first
  if (util.isRegExp(actual) && util.isRegExp(expected)) {
    if (!areSimilarRegExps(actual, expected)) {
      return false;
    } else if (!strict) {
      return true;  // Skip further checks for non-strict comparison.
    }
  }

  // Ensure reflexivity of deepEqual with `arguments` objects.
  // See https://github.com/nodejs/node-v0.x-archive/pull/7178
  if (isArguments(actualTag) !== isArguments(expectedTag)) {
    return false;
  }

  // Check typed arrays and buffers by comparing the content in their
  // underlying ArrayBuffer. This optimization requires that it's
  // reasonable to interpret their underlying memory in the same way,
  // which is checked by comparing their type tags.
  // (e.g. a Uint8Array and a Uint16Array with the same memory content
  // could still be different because they will be interpreted differently)
  // Never perform binary comparisons for Float*Arrays, though,
  // since e.g. +0 === -0 is true despite the two values' bit patterns
  // not being identical.
  if (ArrayBuffer.isView(actual) && ArrayBuffer.isView(expected) &&
      actualTag === expectedTag && !isFloatTypedArrayTag(actualTag)) {
    if (!areSimilarTypedArrays(actual, expected)) {
      return false;
    } else if (!strict) {
      return true;  // Skip further checks for non-strict comparison.
    }

    // Buffer.compare returns true, so actual.length === expected.length
    // if they both only contain numeric keys, we don't need to exam further
    if (Object.keys(actual).length === actual.length &&
        Object.keys(expected).length === expected.length) {
      return true;
    }
  }

  // For all other Object pairs, including Array objects,
  // equivalence is determined by having:
  // a) The same number of owned enumerable properties
  // b) The same set of keys/indexes (although not necessarily the same order)
  // c) Equivalent values for every corresponding key/index
  // Note: this accounts for both named and indexed properties on Arrays.

  // Use memos to handle cycles.
  memos = memos || { actual: [], expected: [] };
  const actualIndex = memos.actual.indexOf(actual);
  if (actualIndex !== -1) {
    if (actualIndex === memos.expected.indexOf(expected)) {
      return true;
    }
  }
  memos.actual.push(actual);
  memos.expected.push(expected);

  return objEquiv(actual, expected, strict, memos);
}

function objEquiv(a, b, strict, actualVisitedObjects) {
  // If one of them is a primitive, the other must be the same.
  if (util.isPrimitive(a) || util.isPrimitive(b))
    return a === b;

  const aKeys = Object.keys(a);
  const bKeys = Object.keys(b);
  var key, i;

  // The pair must have the same number of owned properties
  // (keys incorporates hasOwnProperty).
  if (aKeys.length !== bKeys.length)
    return false;

  // The pair must have the same set of keys (although not
  // necessarily in the same order).
  aKeys.sort();
  bKeys.sort();
  // Cheap key test:
  for (i = aKeys.length - 1; i >= 0; i--) {
    if (aKeys[i] !== bKeys[i])
      return false;
  }

  // The pair must have equivalent values for every corresponding key.
  // Possibly expensive deep test:
  for (i = aKeys.length - 1; i >= 0; i--) {
    key = aKeys[i];
    if (!_deepEqual(a[key], b[key], strict, actualVisitedObjects))
      return false;
  }
  return true;
}

// The non-equivalence assertion tests for any deep inequality.
// assert.notDeepEqual(actual, expected, message_opt);

assert.notDeepEqual = function notDeepEqual(actual, expected, message) {
  if (_deepEqual(actual, expected, false)) {
    fail(actual, expected, message, 'notDeepEqual', assert.notDeepEqual);
  }
};

assert.notDeepStrictEqual = notDeepStrictEqual;
function notDeepStrictEqual(actual, expected, message) {
  if (_deepEqual(actual, expected, true)) {
    fail(actual, expected, message, 'notDeepStrictEqual', notDeepStrictEqual);
  }
}

// The strict equality assertion tests strict equality, as determined by ===.
// assert.strictEqual(actual, expected, message_opt);

assert.strictEqual = function strictEqual(actual, expected, message) {
  if (actual !== expected) {
    fail(actual, expected, message, '===', assert.strictEqual);
  }
};

// The strict non-equality assertion tests for strict inequality, as
// determined by !==.
// assert.notStrictEqual(actual, expected, message_opt);

assert.notStrictEqual = function notStrictEqual(actual, expected, message) {
  if (actual === expected) {
    fail(actual, expected, message, '!==', assert.notStrictEqual);
  }
};

function expectedException(actual, expected) {
  // actual is guaranteed to be an Error object, but we need to check expected.
  if (!expected) {
    return false;
  }

  if (objectToString(expected) === '[object RegExp]') {
    return expected.test(actual);
  }

  try {
    if (actual instanceof expected) {
      return true;
    }
  } catch (e) {
    // Ignore. The instanceof check doesn't work for arrow functions.
  }

  if (Error.isPrototypeOf(expected)) {
    return false;
  }

  return expected.call({}, actual) === true;
}

function _tryBlock(block) {
  var error;
  try {
    block();
  } catch (e) {
    error = e;
  }
  return error;
}

function _throws(shouldThrow, block, expected, message) {
  var actual;

  if (typeof block !== 'function') {
    throw new TypeError('"block" argument must be a function');
  }

  if (typeof expected === 'string') {
    message = expected;
    expected = null;
  }

  actual = _tryBlock(block);

  message = (expected && expected.name ? ' (' + expected.name + ')' : '') +
            (message ? ': ' + message : '.');

  if (shouldThrow && !actual) {
    fail(actual, expected, 'Missing expected exception' + message);
  }

  const userProvidedMessage = typeof message === 'string';
  const isUnwantedException = !shouldThrow && util.isError(actual);
  const isUnexpectedException = !shouldThrow && actual && !expected;

  if ((isUnwantedException &&
      userProvidedMessage &&
      expectedException(actual, expected)) ||
      isUnexpectedException) {
    fail(actual, expected, 'Got unwanted exception' + message);
  }

  if ((shouldThrow && actual && expected &&
      !expectedException(actual, expected)) || (!shouldThrow && actual)) {
    throw actual;
  }
}

// Expected to throw an error.
// assert.throws(block, Error_opt, message_opt);

assert.throws = function throws(block, /*optional*/error, /*optional*/message) {
  _throws(true, block, error, message);
};

// EXTENSION! This is annoying to write outside this module.
assert.doesNotThrow = doesNotThrow;
function doesNotThrow(block, /*optional*/error, /*optional*/message) {
  _throws(false, block, error, message);
}

assert.ifError = function ifError(err) { if (err) throw err; };
