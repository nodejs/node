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

const { compare } = process.binding('buffer');
const util = require('util');
const { isSet, isMap } = process.binding('util');
const { objectToString } = require('internal/util');
const { Buffer } = require('buffer');
const errors = require('internal/errors');

// The assert module provides functions that throw
// AssertionError's when particular conditions are not met. The
// assert module must conform to the following interface.

const assert = module.exports = ok;

// All of the following functions must throw an AssertionError
// when a corresponding condition is not met, with a message that
// may be undefined if not provided. All assertion methods provide
// both the actual and expected values to the assertion error for
// display purposes.

function innerFail(actual, expected, message, operator, stackStartFunction) {
  throw new errors.AssertionError({
    message,
    actual,
    expected,
    operator,
    stackStartFunction
  });
}

function fail(actual, expected, message, operator, stackStartFunction) {
  if (arguments.length === 1)
    message = actual;
  if (arguments.length === 2)
    operator = '!=';
  innerFail(actual, expected, message, operator, stackStartFunction || fail);
}
assert.fail = fail;

// The AssertionError is defined in internal/error.
// new assert.AssertionError({ message: message,
//                             actual: actual,
//                             expected: expected });
assert.AssertionError = errors.AssertionError;


// Pure assertion tests whether a value is truthy, as determined
// by !!value.
function ok(value, message) {
  if (!value) innerFail(value, true, message, '==', ok);
}
assert.ok = ok;

// The equality assertion tests shallow, coercive equality with ==.
/* eslint-disable no-restricted-properties */
assert.equal = function equal(actual, expected, message) {
  // eslint-disable-next-line eqeqeq
  if (actual != expected) innerFail(actual, expected, message, '==', equal);
};

// The non-equality assertion tests for whether two objects are not
// equal with !=.
assert.notEqual = function notEqual(actual, expected, message) {
  // eslint-disable-next-line eqeqeq
  if (actual == expected) {
    innerFail(actual, expected, message, '!=', notEqual);
  }
};

// The equivalence assertion tests a deep equality relation.
assert.deepEqual = function deepEqual(actual, expected, message) {
  if (!innerDeepEqual(actual, expected, false)) {
    innerFail(actual, expected, message, 'deepEqual', deepEqual);
  }
};
/* eslint-enable */

assert.deepStrictEqual = function deepStrictEqual(actual, expected, message) {
  if (!innerDeepEqual(actual, expected, true)) {
    innerFail(actual, expected, message, 'deepStrictEqual', deepStrictEqual);
  }
};

// Check if they have the same source and flags
function areSimilarRegExps(a, b) {
  return a.source === b.source && a.flags === b.flags;
}

// For small buffers it's faster to compare the buffer in a loop. The c++
// barrier including the Buffer.from operation takes the advantage of the faster
// compare otherwise. 300 was the number after which compare became faster.
function areSimilarTypedArrays(a, b) {
  const len = a.byteLength;
  if (len !== b.byteLength) {
    return false;
  }
  if (len < 300) {
    for (var offset = 0; offset < len; offset++) {
      if (a[offset] !== b[offset]) {
        return false;
      }
    }
    return true;
  }
  return compare(Buffer.from(a.buffer,
                             a.byteOffset,
                             len),
                 Buffer.from(b.buffer,
                             b.byteOffset,
                             b.byteLength)) === 0;
}

function isFloatTypedArrayTag(tag) {
  return tag === '[object Float32Array]' || tag === '[object Float64Array]';
}

function isArguments(tag) {
  return tag === '[object Arguments]';
}

function isObjectOrArrayTag(tag) {
  return tag === '[object Array]' || tag === '[object Object]';
}

// Notes: Type tags are historical [[Class]] properties that can be set by
// FunctionTemplate::SetClassName() in C++ or Symbol.toStringTag in JS
// and retrieved using Object.prototype.toString.call(obj) in JS
// See https://tc39.github.io/ecma262/#sec-object.prototype.tostring
// for a list of tags pre-defined in the spec.
// There are some unspecified tags in the wild too (e.g. typed array tags).
// Since tags can be altered, they only serve fast failures
//
// Typed arrays and buffers are checked by comparing the content in their
// underlying ArrayBuffer. This optimization requires that it's
// reasonable to interpret their underlying memory in the same way,
// which is checked by comparing their type tags.
// (e.g. a Uint8Array and a Uint16Array with the same memory content
// could still be different because they will be interpreted differently)
// Never perform binary comparisons for Float*Arrays, though,
// since e.g. +0 === -0 is true despite the two values' bit patterns
// not being identical.
//
// For strict comparison, objects should have
// a) The same built-in type tags
// b) The same prototypes.
function strictDeepEqual(actual, expected) {
  if (actual === null || expected === null ||
    typeof actual !== 'object' || typeof expected !== 'object') {
    return false;
  }
  const actualTag = objectToString(actual);
  const expectedTag = objectToString(expected);

  if (actualTag !== expectedTag) {
    return false;
  }
  if (Object.getPrototypeOf(actual) !== Object.getPrototypeOf(expected)) {
    return false;
  }
  if (isObjectOrArrayTag(actualTag)) {
    // Skip testing the part below and continue in the callee function.
    return;
  }
  if (util.isDate(actual)) {
    if (actual.getTime() !== expected.getTime()) {
      return false;
    }
  } else if (util.isRegExp(actual)) {
    if (!areSimilarRegExps(actual, expected)) {
      return false;
    }
  } else if (!isFloatTypedArrayTag(actualTag) && ArrayBuffer.isView(actual)) {
    if (!areSimilarTypedArrays(actual, expected)) {
      return false;
    }

    // Buffer.compare returns true, so actual.length === expected.length
    // if they both only contain numeric keys, we don't need to exam further
    if (Object.keys(actual).length === actual.length &&
        Object.keys(expected).length === expected.length) {
      return true;
    }
  }
}

function looseDeepEqual(actual, expected) {
  if (actual === null || typeof actual !== 'object') {
    if (expected === null || typeof expected !== 'object') {
      // eslint-disable-next-line eqeqeq
      return actual == expected;
    }
    return false;
  }
  if (expected === null || typeof expected !== 'object') {
    return false;
  }
  if (util.isDate(actual) && util.isDate(expected)) {
    return actual.getTime() === expected.getTime();
  }
  if (util.isRegExp(actual) && util.isRegExp(expected)) {
    return areSimilarRegExps(actual, expected);
  }
  const actualTag = objectToString(actual);
  const expectedTag = objectToString(expected);
  if (actualTag === expectedTag) {
    if (!isObjectOrArrayTag(actualTag) && !isFloatTypedArrayTag(actualTag) &&
      ArrayBuffer.isView(actual)) {
      return areSimilarTypedArrays(actual, expected);
    }
  // Ensure reflexivity of deepEqual with `arguments` objects.
  // See https://github.com/nodejs/node-v0.x-archive/pull/7178
  } else if (isArguments(actualTag) || isArguments(expectedTag)) {
    return false;
  }
}

function innerDeepEqual(actual, expected, strict, memos) {
  // All identical values are equivalent, as determined by ===.
  if (actual === expected) {
    return true;
  }

  // Returns a boolean if (not) equal and undefined in case we have to check
  // further.
  const partialCheck = strict ?
    strictDeepEqual(actual, expected) :
    looseDeepEqual(actual, expected);

  if (partialCheck !== undefined) {
    return partialCheck;
  }

  // For all remaining Object pairs, including Array, objects and Maps,
  // equivalence is determined by having:
  // a) The same number of owned enumerable properties
  // b) The same set of keys/indexes (although not necessarily the same order)
  // c) Equivalent values for every corresponding key/index
  // d) For Sets and Maps, equal contents
  // Note: this accounts for both named and indexed properties on Arrays.

  // Use memos to handle cycles.
  if (memos === undefined) {
    memos = {
      actual: new Map(),
      expected: new Map(),
      position: 0
    };
  } else {
    if (memos.actual.has(actual)) {
      return memos.actual.get(actual) === memos.expected.get(expected);
    }
    memos.position++;
  }

  const aKeys = Object.keys(actual);
  const bKeys = Object.keys(expected);
  var i;

  // The pair must have the same number of owned properties
  // (keys incorporates hasOwnProperty).
  if (aKeys.length !== bKeys.length)
    return false;

  // Cheap key test:
  const keys = {};
  for (i = 0; i < aKeys.length; i++) {
    keys[aKeys[i]] = true;
  }
  for (i = 0; i < aKeys.length; i++) {
    if (keys[bKeys[i]] === undefined)
      return false;
  }

  memos.actual.set(actual, memos.position);
  memos.expected.set(expected, memos.position);

  const areEq = objEquiv(actual, expected, strict, aKeys, memos);

  memos.actual.delete(actual);
  memos.expected.delete(expected);

  return areEq;
}

function setHasEqualElement(set, val1, strict, memo) {
  // Go looking.
  for (const val2 of set) {
    if (innerDeepEqual(val1, val2, strict, memo)) {
      // Remove the matching element to make sure we do not check that again.
      set.delete(val2);
      return true;
    }
  }

  return false;
}

// Note: we actually run this multiple times for each loose key!
// This is done to prevent slowing down the average case.
function setHasLoosePrim(a, b, val) {
  const altValues = findLooseMatchingPrimitives(val);
  if (altValues === undefined)
    return false;

  var matches = 1;
  for (var i = 0; i < altValues.length; i++) {
    if (b.has(altValues[i])) {
      matches--;
    }
    if (a.has(altValues[i])) {
      matches++;
    }
  }
  return matches === 0;
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

  // This is a lazily initiated Set of entries which have to be compared
  // pairwise.
  var set = null;
  for (const val of a) {
    // Note: Checking for the objects first improves the performance for object
    // heavy sets but it is a minor slow down for primitives. As they are fast
    // to check this improves the worst case scenario instead.
    if (typeof val === 'object' && val !== null) {
      if (set === null) {
        set = new Set();
      }
      // If the specified value doesn't exist in the second set its an not null
      // object (or non strict only: a not matching primitive) we'll need to go
      // hunting for something thats deep-(strict-)equal to it. To make this
      // O(n log n) complexity we have to copy these values in a new set first.
      set.add(val);
    } else if (!b.has(val) && (strict || !setHasLoosePrim(a, b, val))) {
      return false;
    }
  }

  if (set !== null) {
    for (const val of b) {
      // We have to check if a primitive value is already
      // matching and only if it's not, go hunting for it.
      if (typeof val === 'object' && val !== null) {
        if (!setHasEqualElement(set, val, strict, memo))
          return false;
      } else if (!a.has(val) && (strict || !setHasLoosePrim(b, a, val))) {
        return false;
      }
    }
  }

  return true;
}

function findLooseMatchingPrimitives(prim) {
  var values, number;
  switch (typeof prim) {
    case 'number':
      values = ['' + prim];
      if (prim === 1 || prim === 0)
        values.push(Boolean(prim));
      return values;
    case 'string':
      number = +prim;
      if ('' + number === prim) {
        values = [number];
        if (number === 1 || number === 0)
          values.push(Boolean(number));
      }
      return values;
    case 'undefined':
      return [null];
    case 'object': // Only pass in null as object!
      return [undefined];
    case 'boolean':
      number = +prim;
      return [number, '' + number];
  }
}

// This is a ugly but relatively fast way to determine if a loose equal entry
// actually has a correspondent matching entry. Otherwise checking for such
// values would be way more expensive (O(n^2)).
// Note: we actually run this multiple times for each loose key!
// This is done to prevent slowing down the average case.
function mapHasLoosePrim(a, b, key1, memo, item1, item2) {
  const altKeys = findLooseMatchingPrimitives(key1);
  if (altKeys === undefined)
    return false;

  const setA = new Set();
  const setB = new Set();

  var keyCount = 1;

  setA.add(item1);
  if (b.has(key1)) {
    keyCount--;
    setB.add(item2);
  }

  for (var i = 0; i < altKeys.length; i++) {
    const key2 = altKeys[i];
    if (a.has(key2)) {
      keyCount++;
      setA.add(a.get(key2));
    }
    if (b.has(key2)) {
      keyCount--;
      setB.add(b.get(key2));
    }
  }
  if (keyCount !== 0 || setA.size !== setB.size)
    return false;

  for (const val of setA) {
    if (typeof val === 'object' && val !== null) {
      if (!setHasEqualElement(setB, val, false, memo))
        return false;
    } else if (!setB.has(val) && !setHasLoosePrim(setA, setB, val)) {
      return false;
    }
  }
  return true;
}

function mapHasEqualEntry(set, map, key1, item1, strict, memo) {
  // To be able to handle cases like:
  //   Map([[{}, 'a'], [{}, 'b']]) vs Map([[{}, 'b'], [{}, 'a']])
  // ... we need to consider *all* matching keys, not just the first we find.
  for (const key2 of set) {
    if (innerDeepEqual(key1, key2, strict, memo) &&
      innerDeepEqual(item1, map.get(key2), strict, memo)) {
      set.delete(key2);
      return true;
    }
  }

  return false;
}

function mapEquiv(a, b, strict, memo) {
  if (a.size !== b.size)
    return false;

  var set = null;

  for (const [key, item1] of a) {
    if (typeof key === 'object' && key !== null) {
      if (set === null) {
        set = new Set();
      }
      set.add(key);
    } else {
      // By directly retrieving the value we prevent another b.has(key) check in
      // almost all possible cases.
      const item2 = b.get(key);
      if ((item2 === undefined && !b.has(key) ||
        !innerDeepEqual(item1, item2, strict, memo)) &&
        (strict || !mapHasLoosePrim(a, b, key, memo, item1, item2))) {
        return false;
      }
    }
  }

  if (set !== null) {
    for (const [key, item] of b) {
      if (typeof key === 'object' && key !== null) {
        if (!mapHasEqualEntry(set, a, key, item, strict, memo))
          return false;
      } else if (!a.has(key) &&
        (strict || !mapHasLoosePrim(b, a, key, memo, item))) {
        return false;
      }
    }
  }

  return true;
}

function objEquiv(a, b, strict, keys, memos) {
  // Sets and maps don't have their entries accessible via normal object
  // properties.
  if (isSet(a)) {
    if (!isSet(b) || !setEquiv(a, b, strict, memos))
      return false;
  } else if (isMap(a)) {
    if (!isMap(b) || !mapEquiv(a, b, strict, memos))
      return false;
  } else if (isSet(b) || isMap(b)) {
    return false;
  }

  // The pair must have equivalent values for every corresponding key.
  // Possibly expensive deep test:
  for (var i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (!innerDeepEqual(a[key], b[key], strict, memos))
      return false;
  }
  return true;
}

// The non-equivalence assertion tests for any deep inequality.
assert.notDeepEqual = function notDeepEqual(actual, expected, message) {
  if (innerDeepEqual(actual, expected, false)) {
    innerFail(actual, expected, message, 'notDeepEqual', notDeepEqual);
  }
};

assert.notDeepStrictEqual = notDeepStrictEqual;
function notDeepStrictEqual(actual, expected, message) {
  if (innerDeepEqual(actual, expected, true)) {
    innerFail(actual, expected, message, 'notDeepStrictEqual',
              notDeepStrictEqual);
  }
}

// The strict equality assertion tests strict equality, as determined by ===.
assert.strictEqual = function strictEqual(actual, expected, message) {
  if (actual !== expected) {
    innerFail(actual, expected, message, '===', strictEqual);
  }
};

// The strict non-equality assertion tests for strict inequality, as
// determined by !==.
assert.notStrictEqual = function notStrictEqual(actual, expected, message) {
  if (actual === expected) {
    innerFail(actual, expected, message, '!==', notStrictEqual);
  }
};

function expectedException(actual, expected) {
  if (typeof expected !== 'function') {
    // Should be a RegExp, if not fail hard
    return expected.test(actual);
  }
  // Guard instanceof against arrow functions as they don't have a prototype.
  if (expected.prototype !== undefined && actual instanceof expected) {
    return true;
  }
  if (Error.isPrototypeOf(expected)) {
    return false;
  }
  return expected.call({}, actual) === true;
}

function tryBlock(block) {
  try {
    block();
  } catch (e) {
    return e;
  }
}

function innerThrows(shouldThrow, block, expected, message) {
  var details = '';

  if (typeof block !== 'function') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'block', 'function',
                               block);
  }

  if (typeof expected === 'string') {
    message = expected;
    expected = null;
  }

  const actual = tryBlock(block);

  if (shouldThrow === true) {
    if (actual === undefined) {
      if (expected && expected.name) {
        details += ` (${expected.name})`;
      }
      details += message ? `: ${message}` : '.';
      fail(actual, expected, `Missing expected exception${details}`, fail);
    }
    if (expected && expectedException(actual, expected) === false) {
      throw actual;
    }
  } else if (actual !== undefined) {
    if (!expected || expectedException(actual, expected)) {
      details = message ? `: ${message}` : '.';
      fail(actual, expected, `Got unwanted exception${details}`, fail);
    }
    throw actual;
  }
}

// Expected to throw an error.
assert.throws = function throws(block, error, message) {
  innerThrows(true, block, error, message);
};

assert.doesNotThrow = function doesNotThrow(block, error, message) {
  innerThrows(false, block, error, message);
};

assert.ifError = function ifError(err) { if (err) throw err; };
