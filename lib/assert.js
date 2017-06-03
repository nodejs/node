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
const { isSet, isMap } = process.binding('util');
const objectToString = require('internal/util').objectToString;
const Buffer = require('buffer').Buffer;

var errors;
function lazyErrors() {
  if (!errors)
    errors = require('internal/errors');
  return errors;
}

// The assert module provides functions that throw
// AssertionError's when particular conditions are not met. The
// assert module must conform to the following interface.

const assert = module.exports = ok;

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
  if (arguments.length === 1)
    message = actual;
  if (arguments.length === 2)
    operator = '!=';
  const errors = lazyErrors();
  throw new errors.AssertionError({
    message: message,
    actual: actual,
    expected: expected,
    operator: operator,
    stackStartFunction: stackStartFunction
  });
}

// EXTENSION! allows for well behaved errors defined elsewhere.
assert.fail = fail;

// The AssertionError is defined in internal/error.
// new assert.AssertionError({ message: message,
//                             actual: actual,
//                             expected: expected });
assert.AssertionError = lazyErrors().AssertionError;


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
  // eslint-disable-next-line eqeqeq
  if (actual != expected) fail(actual, expected, message, '==', assert.equal);
};

// The non-equality assertion tests for whether two objects are not
// equal with !=.
// assert.notEqual(actual, expected, message_opt);

assert.notEqual = function notEqual(actual, expected, message) {
  // eslint-disable-next-line eqeqeq
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
    // eslint-disable-next-line eqeqeq
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

  // For all other Object pairs, including Array objects and Maps,
  // equivalence is determined by having:
  // a) The same number of owned enumerable properties
  // b) The same set of keys/indexes (although not necessarily the same order)
  // c) Equivalent values for every corresponding key/index
  // d) For Sets and Maps, equal contents
  // Note: this accounts for both named and indexed properties on Arrays.

  // Use memos to handle cycles.
  if (!memos) {
    memos = {
      actual: new Map(),
      expected: new Map(),
      position: 0
    };
  } else {
    memos.position++;
  }

  if (memos.actual.has(actual)) {
    return memos.actual.get(actual) === memos.expected.get(expected);
  }

  memos.actual.set(actual, memos.position);
  memos.expected.set(expected, memos.position);

  const areEq = objEquiv(actual, expected, strict, memos);

  memos.actual.delete(actual);
  memos.expected.delete(expected);

  return areEq;
}

function setHasSimilarElement(set, val1, usedEntries, strict, memo) {
  if (set.has(val1)) {
    if (usedEntries)
      usedEntries.add(val1);
    return true;
  }

  // In strict mode the only things which can match a primitive or a function
  // will already be detected by set.has(val1).
  if (strict && (util.isPrimitive(val1) || util.isFunction(val1)))
    return false;

  // Otherwise go looking.
  for (const val2 of set) {
    if (usedEntries && usedEntries.has(val2))
      continue;

    if (_deepEqual(val1, val2, strict, memo)) {
      if (usedEntries)
        usedEntries.add(val2);
      return true;
    }
  }

  return false;
}

function setEquiv(a, b, strict, memo) {
  // This code currently returns false for this pair of sets:
  //   assert.deepEqual(new Set(['1', 1]), new Set([1]))
  //
  // In theory, all the items in the first set have a corresponding == value in
  // the second set, but the sets have different sizes. Its a silly case,
  // and more evidence that deepStrictEqual should always be preferred over
  // deepEqual.
  if (a.size !== b.size)
    return false;

  // This is a set of the entries in b which have been consumed in our pairwise
  // comparison.
  //
  // When the sets contain only value types (eg, lots of numbers), and we're in
  // strict mode, we don't need to match off the entries in a pairwise way. In
  // that case this initialization is done lazily to avoid the allocation &
  // bookkeeping cost. Unfortunately, we can't get away with that in non-strict
  // mode.
  let usedEntries = null;

  for (const val1 of a) {
    if (usedEntries == null && (!strict || typeof val1 === 'object'))
      usedEntries = new Set();

    // If the value doesn't exist in the second set by reference, and its an
    // object or an array we'll need to go hunting for something thats
    // deep-equal to it. Note that this is O(n^2) complexity, and will get
    // slower if large, very similar sets / maps are nested inside.
    // Unfortunately there's no real way around this.
    if (!setHasSimilarElement(b, val1, usedEntries, strict, memo))
      return false;
  }

  return true;
}

function mapHasSimilarEntry(map, key1, item1, usedEntries, strict, memo) {
  // To be able to handle cases like:
  //   Map([[1, 'a'], ['1', 'b']]) vs Map([['1', 'a'], [1, 'b']])
  // or:
  //   Map([[{}, 'a'], [{}, 'b']]) vs Map([[{}, 'b'], [{}, 'a']])
  // ... we need to consider *all* matching keys, not just the first we find.

  // This check is not strictly necessary. The loop performs this check, but
  // doing it here improves performance of the common case when reference-equal
  // keys exist (which includes all primitive-valued keys).
  if (map.has(key1) && _deepEqual(item1, map.get(key1), strict, memo)) {
    if (usedEntries)
      usedEntries.add(key1);
    return true;
  }

  if (strict && (util.isPrimitive(key1) || util.isFunction(key1)))
    return false;

  for (const [key2, item2] of map) {
    // This case is checked above.
    if (key2 === key1)
      continue;

    if (usedEntries && usedEntries.has(key2))
      continue;

    if (_deepEqual(key1, key2, strict, memo) &&
        _deepEqual(item1, item2, strict, memo)) {
      if (usedEntries)
        usedEntries.add(key2);
      return true;
    }
  }

  return false;
}

function mapEquiv(a, b, strict, memo) {
  // Caveat: In non-strict mode, this implementation does not handle cases
  // where maps contain two equivalent-but-not-reference-equal keys.
  //
  // For example, maps like this are currently considered not equivalent:
  if (a.size !== b.size)
    return false;

  let usedEntries = null;

  for (const [key1, item1] of a) {
    if (usedEntries == null && (!strict || typeof key1 === 'object'))
      usedEntries = new Set();

    // Just like setEquiv above, this hunt makes this function O(n^2) when
    // using objects and lists as keys
    if (!mapHasSimilarEntry(b, key1, item1, usedEntries, strict, memo))
      return false;
  }

  return true;
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

  // Sets and maps don't have their entries accessible via normal object
  // properties.
  if (isSet(a)) {
    if (!isSet(b) || !setEquiv(a, b, strict, actualVisitedObjects))
      return false;
  } else if (isSet(b)) {
    return false;
  }

  if (isMap(a)) {
    if (!isMap(b) || !mapEquiv(a, b, strict, actualVisitedObjects))
      return false;
  } else if (isMap(b)) {
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
    const errors = lazyErrors();
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'block', 'function',
                               typeof block);
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
