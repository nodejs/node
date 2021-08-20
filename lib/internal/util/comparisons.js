'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypePush,
  BigIntPrototypeValueOf,
  BooleanPrototypeValueOf,
  DatePrototypeGetTime,
  Error,
  NumberIsNaN,
  NumberPrototypeValueOf,
  ObjectGetOwnPropertySymbols,
  ObjectGetPrototypeOf,
  ObjectIs,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectPrototypePropertyIsEnumerable,
  ObjectPrototypeToString,
  SafeMap,
  SafeSet,
  StringPrototypeValueOf,
  SymbolPrototypeValueOf,
  TypedArrayPrototypeGetSymbolToStringTag,
  Uint8Array,
} = primordials;

const { compare } = internalBinding('buffer');
const assert = require('internal/assert');
const types = require('internal/util/types');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
  isDate,
  isMap,
  isRegExp,
  isSet,
  isNativeError,
  isBoxedPrimitive,
  isNumberObject,
  isStringObject,
  isBooleanObject,
  isBigIntObject,
  isSymbolObject,
  isFloat32Array,
  isFloat64Array,
} = types;
const {
  getOwnNonIndexProperties,
  propertyFilter: {
    ONLY_ENUMERABLE,
    SKIP_SYMBOLS
  }
} = internalBinding('util');

const kStrict = true;
const kLoose = false;

const kNoIterator = 0;
const kIsArray = 1;
const kIsSet = 2;
const kIsMap = 3;

// Check if they have the same source and flags
function areSimilarRegExps(a, b) {
  return a.source === b.source && a.flags === b.flags;
}

function areSimilarFloatArrays(a, b) {
  if (a.byteLength !== b.byteLength) {
    return false;
  }
  for (let offset = 0; offset < a.byteLength; offset++) {
    if (a[offset] !== b[offset]) {
      return false;
    }
  }
  return true;
}

function areSimilarTypedArrays(a, b) {
  if (a.byteLength !== b.byteLength) {
    return false;
  }
  return compare(new Uint8Array(a.buffer, a.byteOffset, a.byteLength),
                 new Uint8Array(b.buffer, b.byteOffset, b.byteLength)) === 0;
}

function areEqualArrayBuffers(buf1, buf2) {
  return buf1.byteLength === buf2.byteLength &&
    compare(new Uint8Array(buf1), new Uint8Array(buf2)) === 0;
}

function isEqualBoxedPrimitive(val1, val2) {
  if (isNumberObject(val1)) {
    return isNumberObject(val2) &&
           ObjectIs(NumberPrototypeValueOf(val1),
                    NumberPrototypeValueOf(val2));
  }
  if (isStringObject(val1)) {
    return isStringObject(val2) &&
           StringPrototypeValueOf(val1) === StringPrototypeValueOf(val2);
  }
  if (isBooleanObject(val1)) {
    return isBooleanObject(val2) &&
           BooleanPrototypeValueOf(val1) === BooleanPrototypeValueOf(val2);
  }
  if (isBigIntObject(val1)) {
    return isBigIntObject(val2) &&
           BigIntPrototypeValueOf(val1) === BigIntPrototypeValueOf(val2);
  }
  if (isSymbolObject(val1)) {
    return isSymbolObject(val2) &&
          SymbolPrototypeValueOf(val1) === SymbolPrototypeValueOf(val2);
  }
  /* c8 ignore next */
  assert.fail(`Unknown boxed type ${val1}`);
}

// Notes: Type tags are historical [[Class]] properties that can be set by
// FunctionTemplate::SetClassName() in C++ or Symbol.toStringTag in JS
// and retrieved using Object.prototype.toString.call(obj) in JS
// See https://tc39.github.io/ecma262/#sec-object.prototype.tostring
// for a list of tags pre-defined in the spec.
// There are some unspecified tags in the wild too (e.g. typed array tags).
// Since tags can be altered, they only serve fast failures
//
// For strict comparison, objects should have
// a) The same built-in type tag.
// b) The same prototypes.

function innerDeepEqual(val1, val2, strict, memos) {
  // All identical values are equivalent, as determined by ===.
  if (val1 === val2) {
    if (val1 !== 0)
      return true;
    return strict ? ObjectIs(val1, val2) : true;
  }

  // Check more closely if val1 and val2 are equal.
  if (strict) {
    if (typeof val1 !== 'object') {
      return typeof val1 === 'number' && NumberIsNaN(val1) &&
        NumberIsNaN(val2);
    }
    if (typeof val2 !== 'object' || val1 === null || val2 === null) {
      return false;
    }
    if (ObjectGetPrototypeOf(val1) !== ObjectGetPrototypeOf(val2)) {
      return false;
    }
  } else {
    if (val1 === null || typeof val1 !== 'object') {
      if (val2 === null || typeof val2 !== 'object') {
        // eslint-disable-next-line eqeqeq
        return val1 == val2 || (NumberIsNaN(val1) && NumberIsNaN(val2));
      }
      return false;
    }
    if (val2 === null || typeof val2 !== 'object') {
      return false;
    }
  }
  const val1Tag = ObjectPrototypeToString(val1);
  const val2Tag = ObjectPrototypeToString(val2);

  if (val1Tag !== val2Tag) {
    return false;
  }

  if (ArrayIsArray(val1)) {
    // Check for sparse arrays and general fast path
    if (!ArrayIsArray(val2) || val1.length !== val2.length) {
      return false;
    }
    const filter = strict ? ONLY_ENUMERABLE : ONLY_ENUMERABLE | SKIP_SYMBOLS;
    const keys1 = getOwnNonIndexProperties(val1, filter);
    const keys2 = getOwnNonIndexProperties(val2, filter);
    if (keys1.length !== keys2.length) {
      return false;
    }
    return keyCheck(val1, val2, strict, memos, kIsArray, keys1);
  } else if (val1Tag === '[object Object]') {
    return keyCheck(val1, val2, strict, memos, kNoIterator);
  } else if (isDate(val1)) {
    if (!isDate(val2) ||
        DatePrototypeGetTime(val1) !== DatePrototypeGetTime(val2)) {
      return false;
    }
  } else if (isRegExp(val1)) {
    if (!isRegExp(val2) || !areSimilarRegExps(val1, val2)) {
      return false;
    }
  } else if (isNativeError(val1) || val1 instanceof Error) {
    // Do not compare the stack as it might differ even though the error itself
    // is otherwise identical.
    if ((!isNativeError(val2) && !(val2 instanceof Error)) ||
        val1.message !== val2.message ||
        val1.name !== val2.name) {
      return false;
    }
  } else if (isArrayBufferView(val1)) {
    if (TypedArrayPrototypeGetSymbolToStringTag(val1) !==
        TypedArrayPrototypeGetSymbolToStringTag(val2)) {
      return false;
    }
    if (!strict && (isFloat32Array(val1) || isFloat64Array(val1))) {
      if (!areSimilarFloatArrays(val1, val2)) {
        return false;
      }
    } else if (!areSimilarTypedArrays(val1, val2)) {
      return false;
    }
    // Buffer.compare returns true, so val1.length === val2.length. If they both
    // only contain numeric keys, we don't need to exam further than checking
    // the symbols.
    const filter = strict ? ONLY_ENUMERABLE : ONLY_ENUMERABLE | SKIP_SYMBOLS;
    const keys1 = getOwnNonIndexProperties(val1, filter);
    const keys2 = getOwnNonIndexProperties(val2, filter);
    if (keys1.length !== keys2.length) {
      return false;
    }
    return keyCheck(val1, val2, strict, memos, kNoIterator, keys1);
  } else if (isSet(val1)) {
    if (!isSet(val2) || val1.size !== val2.size) {
      return false;
    }
    return keyCheck(val1, val2, strict, memos, kIsSet);
  } else if (isMap(val1)) {
    if (!isMap(val2) || val1.size !== val2.size) {
      return false;
    }
    return keyCheck(val1, val2, strict, memos, kIsMap);
  } else if (isAnyArrayBuffer(val1)) {
    if (!isAnyArrayBuffer(val2) || !areEqualArrayBuffers(val1, val2)) {
      return false;
    }
  } else if (isBoxedPrimitive(val1)) {
    if (!isEqualBoxedPrimitive(val1, val2)) {
      return false;
    }
  } else if (ArrayIsArray(val2) ||
             isArrayBufferView(val2) ||
             isSet(val2) ||
             isMap(val2) ||
             isDate(val2) ||
             isRegExp(val2) ||
             isAnyArrayBuffer(val2) ||
             isBoxedPrimitive(val2) ||
             isNativeError(val2) ||
             val2 instanceof Error) {
    return false;
  }
  return keyCheck(val1, val2, strict, memos, kNoIterator);
}

function getEnumerables(val, keys) {
  return ArrayPrototypeFilter(
    keys,
    (k) => ObjectPrototypePropertyIsEnumerable(val, k)
  );
}

function keyCheck(val1, val2, strict, memos, iterationType, aKeys) {
  // For all remaining Object pairs, including Array, objects and Maps,
  // equivalence is determined by having:
  // a) The same number of owned enumerable properties
  // b) The same set of keys/indexes (although not necessarily the same order)
  // c) Equivalent values for every corresponding key/index
  // d) For Sets and Maps, equal contents
  // Note: this accounts for both named and indexed properties on Arrays.
  if (arguments.length === 5) {
    aKeys = ObjectKeys(val1);
    const bKeys = ObjectKeys(val2);

    // The pair must have the same number of owned properties.
    if (aKeys.length !== bKeys.length) {
      return false;
    }
  }

  // Cheap key test
  let i = 0;
  for (; i < aKeys.length; i++) {
    if (!ObjectPrototypePropertyIsEnumerable(val2, aKeys[i])) {
      return false;
    }
  }

  if (strict && arguments.length === 5) {
    const symbolKeysA = ObjectGetOwnPropertySymbols(val1);
    if (symbolKeysA.length !== 0) {
      let count = 0;
      for (i = 0; i < symbolKeysA.length; i++) {
        const key = symbolKeysA[i];
        if (ObjectPrototypePropertyIsEnumerable(val1, key)) {
          if (!ObjectPrototypePropertyIsEnumerable(val2, key)) {
            return false;
          }
          ArrayPrototypePush(aKeys, key);
          count++;
        } else if (ObjectPrototypePropertyIsEnumerable(val2, key)) {
          return false;
        }
      }
      const symbolKeysB = ObjectGetOwnPropertySymbols(val2);
      if (symbolKeysA.length !== symbolKeysB.length &&
          getEnumerables(val2, symbolKeysB).length !== count) {
        return false;
      }
    } else {
      const symbolKeysB = ObjectGetOwnPropertySymbols(val2);
      if (symbolKeysB.length !== 0 &&
          getEnumerables(val2, symbolKeysB).length !== 0) {
        return false;
      }
    }
  }

  if (aKeys.length === 0 &&
      (iterationType === kNoIterator ||
        (iterationType === kIsArray && val1.length === 0) ||
        val1.size === 0)) {
    return true;
  }

  // Use memos to handle cycles.
  if (memos === undefined) {
    memos = {
      val1: new SafeMap(),
      val2: new SafeMap(),
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

  const areEq = objEquiv(val1, val2, strict, aKeys, memos, iterationType);

  memos.val1.delete(val1);
  memos.val2.delete(val2);

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

// See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Equality_comparisons_and_sameness#Loose_equality_using
// Sadly it is not possible to detect corresponding values properly in case the
// type is a string, number, bigint or boolean. The reason is that those values
// can match lots of different string values (e.g., 1n == '+00001').
function findLooseMatchingPrimitives(prim) {
  switch (typeof prim) {
    case 'undefined':
      return null;
    case 'object': // Only pass in null as object!
      return undefined;
    case 'symbol':
      return false;
    case 'string':
      prim = +prim;
      // Loose equal entries exist only if the string is possible to convert to
      // a regular number and not NaN.
      // Fall through
    case 'number':
      if (NumberIsNaN(prim)) {
        return false;
      }
  }
  return true;
}

function setMightHaveLoosePrim(a, b, prim) {
  const altValue = findLooseMatchingPrimitives(prim);
  if (altValue != null)
    return altValue;

  return b.has(altValue) && !a.has(altValue);
}

function mapMightHaveLoosePrim(a, b, prim, item, memo) {
  const altValue = findLooseMatchingPrimitives(prim);
  if (altValue != null) {
    return altValue;
  }
  const curB = b.get(altValue);
  if ((curB === undefined && !b.has(altValue)) ||
      !innerDeepEqual(item, curB, false, memo)) {
    return false;
  }
  return !a.has(altValue) && innerDeepEqual(item, curB, false, memo);
}

function setEquiv(a, b, strict, memo) {
  // This is a lazily initiated Set of entries which have to be compared
  // pairwise.
  let set = null;
  for (const val of a) {
    // Note: Checking for the objects first improves the performance for object
    // heavy sets but it is a minor slow down for primitives. As they are fast
    // to check this improves the worst case scenario instead.
    if (typeof val === 'object' && val !== null) {
      if (set === null) {
        set = new SafeSet();
      }
      // If the specified value doesn't exist in the second set it's a non-null
      // object (or non strict only: a not matching primitive) we'll need to go
      // hunting for something that's deep-(strict-)equal to it. To make this
      // O(n log n) complexity we have to copy these values in a new set first.
      set.add(val);
    } else if (!b.has(val)) {
      if (strict)
        return false;

      // Fast path to detect missing string, symbol, undefined and null values.
      if (!setMightHaveLoosePrim(a, b, val)) {
        return false;
      }

      if (set === null) {
        set = new SafeSet();
      }
      set.add(val);
    }
  }

  if (set !== null) {
    for (const val of b) {
      // We have to check if a primitive value is already
      // matching and only if it's not, go hunting for it.
      if (typeof val === 'object' && val !== null) {
        if (!setHasEqualElement(set, val, strict, memo))
          return false;
      } else if (!strict &&
                 !a.has(val) &&
                 !setHasEqualElement(set, val, strict, memo)) {
        return false;
      }
    }
    return set.size === 0;
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
  let set = null;

  for (const { 0: key, 1: item1 } of a) {
    if (typeof key === 'object' && key !== null) {
      if (set === null) {
        set = new SafeSet();
      }
      set.add(key);
    } else {
      // By directly retrieving the value we prevent another b.has(key) check in
      // almost all possible cases.
      const item2 = b.get(key);
      if (((item2 === undefined && !b.has(key)) ||
          !innerDeepEqual(item1, item2, strict, memo))) {
        if (strict)
          return false;
        // Fast path to detect missing string, symbol, undefined and null
        // keys.
        if (!mapMightHaveLoosePrim(a, b, key, item1, memo))
          return false;
        if (set === null) {
          set = new SafeSet();
        }
        set.add(key);
      }
    }
  }

  if (set !== null) {
    for (const { 0: key, 1: item } of b) {
      if (typeof key === 'object' && key !== null) {
        if (!mapHasEqualEntry(set, a, key, item, strict, memo))
          return false;
      } else if (!strict &&
                 (!a.has(key) ||
                   !innerDeepEqual(a.get(key), item, false, memo)) &&
                 !mapHasEqualEntry(set, a, key, item, false, memo)) {
        return false;
      }
    }
    return set.size === 0;
  }

  return true;
}

function objEquiv(a, b, strict, keys, memos, iterationType) {
  // Sets and maps don't have their entries accessible via normal object
  // properties.
  let i = 0;

  if (iterationType === kIsSet) {
    if (!setEquiv(a, b, strict, memos)) {
      return false;
    }
  } else if (iterationType === kIsMap) {
    if (!mapEquiv(a, b, strict, memos)) {
      return false;
    }
  } else if (iterationType === kIsArray) {
    for (; i < a.length; i++) {
      if (ObjectPrototypeHasOwnProperty(a, i)) {
        if (!ObjectPrototypeHasOwnProperty(b, i) ||
            !innerDeepEqual(a[i], b[i], strict, memos)) {
          return false;
        }
      } else if (ObjectPrototypeHasOwnProperty(b, i)) {
        return false;
      } else {
        // Array is sparse.
        const keysA = ObjectKeys(a);
        for (; i < keysA.length; i++) {
          const key = keysA[i];
          if (!ObjectPrototypeHasOwnProperty(b, key) ||
              !innerDeepEqual(a[key], b[key], strict, memos)) {
            return false;
          }
        }
        if (keysA.length !== ObjectKeys(b).length) {
          return false;
        }
        return true;
      }
    }
  }

  // The pair must have equivalent values for every corresponding key.
  // Possibly expensive deep test:
  for (i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (!innerDeepEqual(a[key], b[key], strict, memos)) {
      return false;
    }
  }
  return true;
}

function isDeepEqual(val1, val2) {
  return innerDeepEqual(val1, val2, kLoose);
}

function isDeepStrictEqual(val1, val2) {
  return innerDeepEqual(val1, val2, kStrict);
}

module.exports = {
  isDeepEqual,
  isDeepStrictEqual
};
