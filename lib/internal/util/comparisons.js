'use strict';

const { compare } = process.binding('buffer');
const { isArrayBufferView } = require('internal/util/types');
const { isDate, isMap, isRegExp, isSet } = process.binding('util');

function objectToString(o) {
  return Object.prototype.toString.call(o);
}

// Check if they have the same source and flags
function areSimilarRegExps(a, b) {
  return a.source === b.source && a.flags === b.flags;
}

// For small buffers it's faster to compare the buffer in a loop. The c++
// barrier including the Uint8Array operation takes the advantage of the faster
// binary compare otherwise. The break even point was at about 300 characters.
function areSimilarTypedArrays(a, b, max) {
  const len = a.byteLength;
  if (len !== b.byteLength) {
    return false;
  }
  if (len < max) {
    for (var offset = 0; offset < len; offset++) {
      if (a[offset] !== b[offset]) {
        return false;
      }
    }
    return true;
  }
  return compare(new Uint8Array(a.buffer, a.byteOffset, len),
                 new Uint8Array(b.buffer, b.byteOffset, b.byteLength)) === 0;
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
// could still be different because they will be interpreted differently).
//
// For strict comparison, objects should have
// a) The same built-in type tags
// b) The same prototypes.
function strictDeepEqual(val1, val2, memos) {
  if (typeof val1 !== 'object') {
    return typeof val1 === 'number' && Number.isNaN(val1) &&
      Number.isNaN(val2);
  }
  if (typeof val2 !== 'object' || val1 === null || val2 === null) {
    return false;
  }
  const val1Tag = objectToString(val1);
  const val2Tag = objectToString(val2);

  if (val1Tag !== val2Tag) {
    return false;
  }
  if (Object.getPrototypeOf(val1) !== Object.getPrototypeOf(val2)) {
    return false;
  }
  if (val1Tag === '[object Array]') {
    // Check for sparse arrays and general fast path
    if (val1.length !== val2.length)
      return false;
    // Skip testing the part below and continue with the keyCheck.
    return keyCheck(val1, val2, true, memos);
  }
  if (val1Tag === '[object Object]') {
    // Skip testing the part below and continue with the keyCheck.
    return keyCheck(val1, val2, true, memos);
  }
  if (isDate(val1)) {
    if (val1.getTime() !== val2.getTime()) {
      return false;
    }
  } else if (isRegExp(val1)) {
    if (!areSimilarRegExps(val1, val2)) {
      return false;
    }
  } else if (val1Tag === '[object Error]') {
    // Do not compare the stack as it might differ even though the error itself
    // is otherwise identical. The non-enumerable name should be identical as
    // the prototype is also identical. Otherwise this is caught later on.
    if (val1.message !== val2.message) {
      return false;
    }
  } else if (isArrayBufferView(val1)) {
    if (!areSimilarTypedArrays(val1, val2,
                               isFloatTypedArrayTag(val1Tag) ? 0 : 300)) {
      return false;
    }
    // Buffer.compare returns true, so val1.length === val2.length
    // if they both only contain numeric keys, we don't need to exam further
    return keyCheck(val1, val2, true, memos, val1.length,
                    val2.length);
  } else if (typeof val1.valueOf === 'function') {
    const val1Value = val1.valueOf();
    // Note: Boxed string keys are going to be compared again by Object.keys
    if (val1Value !== val1) {
      if (!innerDeepEqual(val1Value, val2.valueOf(), true))
        return false;
      // Fast path for boxed primitives
      var lengthval1 = 0;
      var lengthval2 = 0;
      if (typeof val1Value === 'string') {
        lengthval1 = val1.length;
        lengthval2 = val2.length;
      }
      return keyCheck(val1, val2, true, memos, lengthval1,
                      lengthval2);
    }
  }
  return keyCheck(val1, val2, true, memos);
}

function looseDeepEqual(val1, val2, memos) {
  if (val1 === null || typeof val1 !== 'object') {
    if (val2 === null || typeof val2 !== 'object') {
      // eslint-disable-next-line eqeqeq
      return val1 == val2;
    }
    return false;
  }
  if (val2 === null || typeof val2 !== 'object') {
    return false;
  }
  if (isDate(val1) && isDate(val2)) {
    return val1.getTime() === val2.getTime();
  }
  if (isRegExp(val1) && isRegExp(val2)) {
    return areSimilarRegExps(val1, val2);
  }
  if (val1 instanceof Error && val2 instanceof Error) {
    if (val1.message !== val2.message || val1.name !== val2.name)
      return false;
  }
  const val1Tag = objectToString(val1);
  const val2Tag = objectToString(val2);
  if (val1Tag === val2Tag) {
    if (!isObjectOrArrayTag(val1Tag) && isArrayBufferView(val1)) {
      return areSimilarTypedArrays(val1, val2,
                                   isFloatTypedArrayTag(val1Tag) ?
                                     Infinity : 300);
    }
  // Ensure reflexivity of deepEqual with `arguments` objects.
  // See https://github.com/nodejs/node-v0.x-archive/pull/7178
  } else if (isArguments(val1Tag) || isArguments(val2Tag)) {
    return false;
  }
  return keyCheck(val1, val2, false, memos);
}

function keyCheck(val1, val2, strict, memos, lengthA, lengthB) {
  // For all remaining Object pairs, including Array, objects and Maps,
  // equivalence is determined by having:
  // a) The same number of owned enumerable properties
  // b) The same set of keys/indexes (although not necessarily the same order)
  // c) Equivalent values for every corresponding key/index
  // d) For Sets and Maps, equal contents
  // Note: this accounts for both named and indexed properties on Arrays.
  var aKeys = Object.keys(val1);
  var bKeys = Object.keys(val2);
  var i;

  // The pair must have the same number of owned properties.
  if (aKeys.length !== bKeys.length)
    return false;

  if (strict) {
    var symbolKeysA = Object.getOwnPropertySymbols(val1);
    var symbolKeysB = Object.getOwnPropertySymbols(val2);
    if (symbolKeysA.length !== 0) {
      symbolKeysA = symbolKeysA.filter((k) =>
        propertyIsEnumerable.call(val1, k));
      symbolKeysB = symbolKeysB.filter((k) =>
        propertyIsEnumerable.call(val2, k));
      if (symbolKeysA.length !== symbolKeysB.length)
        return false;
    } else if (symbolKeysB.length !== 0 && symbolKeysB.filter((k) =>
      propertyIsEnumerable.call(val2, k)).length !== 0) {
      return false;
    }
    if (lengthA !== undefined) {
      if (aKeys.length !== lengthA || bKeys.length !== lengthB)
        return false;
      if (symbolKeysA.length === 0)
        return true;
      aKeys = [];
      bKeys = [];
    }
    if (symbolKeysA.length !== 0) {
      aKeys.push(...symbolKeysA);
      bKeys.push(...symbolKeysB);
    }
  }

  // Cheap key test:
  const keys = {};
  for (i = 0; i < aKeys.length; i++) {
    keys[aKeys[i]] = true;
  }
  for (i = 0; i < aKeys.length; i++) {
    if (keys[bKeys[i]] === undefined)
      return false;
  }

  // Use memos to handle cycles.
  if (memos === undefined) {
    memos = {
      val1: new Map(),
      val2: new Map(),
      position: 0
    };
  } else {
    // We prevent up to two map.has(x) calls by directly retrieving the value
    // and checking for undefined. The map can only contain numbers, so it is
    // safe to check for undefined only.
    const val2MemoA = memos.val1.get(val1);
    if (val2MemoA !== undefined) {
      const val2MemoB = memos.val2.get(val2);
      if (val2MemoB !== undefined) {
        return val2MemoA === val2MemoB;
      }
    }
    memos.position++;
  }

  memos.val1.set(val1, memos.position);
  memos.val2.set(val2, memos.position);

  const areEq = objEquiv(val1, val2, strict, aKeys, memos);

  memos.val1.delete(val1);
  memos.val2.delete(val2);

  return areEq;
}

function innerDeepEqual(val1, val2, strict, memos) {
  // All identical values are equivalent, as determined by ===.
  if (val1 === val2) {
    if (val1 !== 0)
      return true;
    return strict ? Object.is(val1, val2) : true;
  }

  // Check more closely if val1 and val2 are equal.
  if (strict === true)
    return strictDeepEqual(val1, val2, memos);

  return looseDeepEqual(val1, val2, memos);
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

// Note: we val1ly run this multiple times for each loose key!
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
// val1ly has a correspondent matching entry. Otherwise checking for such
// values would be way more expensive (O(n^2)).
// Note: we val1ly run this multiple times for each loose key!
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

function isDeepEqual(val1, val2) {
  return innerDeepEqual(val1, val2, false);
}

function isDeepStrictEqual(val1, val2) {
  return innerDeepEqual(val1, val2, true);
}

module.exports = {
  isDeepEqual,
  isDeepStrictEqual
};
