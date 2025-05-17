'use strict';

const {
  Array,
  ArrayBuffer,
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypePush,
  BigInt,
  BigInt64Array,
  BigIntPrototypeValueOf,
  BigUint64Array,
  Boolean,
  BooleanPrototypeValueOf,
  DataView,
  Date,
  DatePrototypeGetTime,
  Error,
  ErrorIsError,
  Float32Array,
  Float64Array,
  Function,
  Int16Array,
  Int32Array,
  Int8Array,
  Map,
  Number,
  NumberPrototypeValueOf,
  Object,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertySymbols: getOwnSymbols,
  ObjectGetPrototypeOf,
  ObjectIs,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty: hasOwn,
  ObjectPrototypePropertyIsEnumerable: hasEnumerable,
  ObjectPrototypeToString,
  Promise,
  RegExp,
  SafeSet,
  Set,
  String,
  StringPrototypeValueOf,
  Symbol,
  SymbolPrototypeValueOf,
  TypedArrayPrototypeGetByteLength: getByteLength,
  TypedArrayPrototypeGetSymbolToStringTag,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
  WeakMap,
  WeakSet,
  globalThis: { Float16Array },
} = primordials;

const { compare } = internalBinding('buffer');
const assert = require('internal/assert');
const { isURL } = require('internal/url');
const { isError } = require('internal/util');
const { Buffer } = require('buffer');

const wellKnownConstructors = new SafeSet()
  .add(Array)
  .add(ArrayBuffer)
  .add(BigInt)
  .add(BigInt64Array)
  .add(BigUint64Array)
  .add(Boolean)
  .add(Buffer)
  .add(DataView)
  .add(Date)
  .add(Error)
  .add(Float32Array)
  .add(Float64Array)
  .add(Function)
  .add(Int16Array)
  .add(Int32Array)
  .add(Int8Array)
  .add(Map)
  .add(Number)
  .add(Object)
  .add(Promise)
  .add(RegExp)
  .add(Set)
  .add(String)
  .add(Symbol)
  .add(Uint16Array)
  .add(Uint32Array)
  .add(Uint8Array)
  .add(Uint8ClampedArray)
  .add(WeakMap)
  .add(WeakSet);

if (Float16Array) { // TODO(BridgeAR): Remove when regularly supported
  wellKnownConstructors.add(Float16Array);
}

const types = require('internal/util/types');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
  isDate,
  isMap,
  isRegExp,
  isSet,
  isBoxedPrimitive,
  isNumberObject,
  isStringObject,
  isBooleanObject,
  isBigIntObject,
  isSymbolObject,
  isFloat16Array,
  isFloat32Array,
  isFloat64Array,
  isKeyObject,
  isCryptoKey,
  isWeakMap,
  isWeakSet,
} = types;
const {
  constants: {
    ONLY_ENUMERABLE,
    SKIP_SYMBOLS,
  },
  getOwnNonIndexProperties,
} = internalBinding('util');

const kStrict = 1;
const kLoose = 0;
const kPartial = 2;

const kNoIterator = 0;
const kIsArray = 1;
const kIsSet = 2;
const kIsMap = 3;

let kKeyObject;

// Check if they have the same source and flags
function areSimilarRegExps(a, b) {
  return a.source === b.source &&
         a.flags === b.flags &&
         a.lastIndex === b.lastIndex;
}

function isPartialUint8Array(a, b) {
  const lenA = getByteLength(a);
  const lenB = getByteLength(b);
  if (lenA < lenB) {
    return false;
  }
  let offsetA = 0;
  for (let offsetB = 0; offsetB < lenB; offsetB++) {
    while (!ObjectIs(a[offsetA], b[offsetB])) {
      offsetA++;
      if (offsetA > lenA - lenB + offsetB) {
        return false;
      }
    }
    offsetA++;
  }
  return true;
}

function isPartialArrayBufferView(a, b) {
  if (a.byteLength < b.byteLength) {
    return false;
  }
  return isPartialUint8Array(
    new Uint8Array(a.buffer, a.byteOffset, a.byteLength),
    new Uint8Array(b.buffer, b.byteOffset, b.byteLength),
  );
}

function areSimilarFloatArrays(a, b) {
  const len = getByteLength(a);
  if (len !== getByteLength(b)) {
    return false;
  }
  for (let offset = 0; offset < len; offset++) {
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

function isEnumerableOrIdentical(val1, val2, prop, mode, memos, method) {
  return hasEnumerable(val2, prop) || // This is handled by Object.keys()
      (mode === kPartial && (val2[prop] === undefined || (prop === 'message' && val2[prop] === ''))) ||
      innerDeepEqual(val1[prop], val2[prop], mode, memos);
}

function innerDeepEqual(val1, val2, mode, memos) {
  // All identical values are equivalent, as determined by ===.
  if (val1 === val2) {
    return val1 !== 0 || ObjectIs(val1, val2) || mode === kLoose;
  }

  // Check more closely if val1 and val2 are equal.
  if (mode !== kLoose) {
    if (typeof val1 === 'number') {
      // Check for NaN
      // eslint-disable-next-line no-self-compare
      return val1 !== val1 && val2 !== val2;
    }
    if (typeof val2 !== 'object' ||
        typeof val1 !== 'object' ||
        val1 === null ||
        val2 === null) {
      return false;
    }
  } else {
    if (val1 === null || typeof val1 !== 'object') {
      return (val2 === null || typeof val2 !== 'object') &&
             // Check for NaN
             // eslint-disable-next-line eqeqeq, no-self-compare
             (val1 == val2 || (val1 !== val1 && val2 !== val2));
    }
    if (val2 === null || typeof val2 !== 'object') {
      return false;
    }
  }
  return objectComparisonStart(val1, val2, mode, memos);
}

function objectComparisonStart(val1, val2, mode, memos) {
  if (mode === kStrict) {
    if (wellKnownConstructors.has(val1.constructor) ||
        (val1.constructor !== undefined && !hasOwn(val1, 'constructor'))) {
      if (val1.constructor !== val2.constructor) {
        return false;
      }
    } else if (ObjectGetPrototypeOf(val1) !== ObjectGetPrototypeOf(val2)) {
      return false;
    }
  }

  const val1Tag = ObjectPrototypeToString(val1);
  const val2Tag = ObjectPrototypeToString(val2);

  if (val1Tag !== val2Tag) {
    return false;
  }

  if (ArrayIsArray(val1)) {
    if (!ArrayIsArray(val2) ||
        (val1.length !== val2.length && (mode !== kPartial || val1.length < val2.length))) {
      return false;
    }

    const filter = mode !== kLoose ? ONLY_ENUMERABLE : ONLY_ENUMERABLE | SKIP_SYMBOLS;
    const keys2 = getOwnNonIndexProperties(val2, filter);
    if (mode !== kPartial &&
        keys2.length !== getOwnNonIndexProperties(val1, filter).length) {
      return false;
    }
    return keyCheck(val1, val2, mode, memos, kIsArray, keys2);
  } else if (val1Tag === '[object Object]') {
    return keyCheck(val1, val2, mode, memos, kNoIterator);
  } else if (isDate(val1)) {
    if (!isDate(val2) ||
        DatePrototypeGetTime(val1) !== DatePrototypeGetTime(val2)) {
      return false;
    }
  } else if (isRegExp(val1)) {
    if (!isRegExp(val2) || !areSimilarRegExps(val1, val2)) {
      return false;
    }
  } else if (isArrayBufferView(val1)) {
    if (TypedArrayPrototypeGetSymbolToStringTag(val1) !==
        TypedArrayPrototypeGetSymbolToStringTag(val2)) {
      return false;
    }
    if (mode === kPartial && val1.byteLength !== val2.byteLength) {
      if (!isPartialArrayBufferView(val1, val2)) {
        return false;
      }
    } else if (mode === kLoose &&
               (isFloat32Array(val1) || isFloat64Array(val1) || isFloat16Array(val1))) {
      if (!areSimilarFloatArrays(val1, val2)) {
        return false;
      }
    } else if (!areSimilarTypedArrays(val1, val2)) {
      return false;
    }
    // Buffer.compare returns true, so val1.length === val2.length. If they both
    // only contain numeric keys, we don't need to exam further than checking
    // the symbols.
    const filter = mode !== kLoose ? ONLY_ENUMERABLE : ONLY_ENUMERABLE | SKIP_SYMBOLS;
    const keys2 = getOwnNonIndexProperties(val2, filter);
    if (mode !== kPartial &&
        keys2.length !== getOwnNonIndexProperties(val1, filter).length) {
      return false;
    }
    return keyCheck(val1, val2, mode, memos, kNoIterator, keys2);
  } else if (isSet(val1)) {
    if (!isSet(val2) ||
        (val1.size !== val2.size && (mode !== kPartial || val1.size < val2.size))) {
      return false;
    }
    return keyCheck(val1, val2, mode, memos, kIsSet);
  } else if (isMap(val1)) {
    if (!isMap(val2) ||
        (val1.size !== val2.size && (mode !== kPartial || val1.size < val2.size))) {
      return false;
    }
    return keyCheck(val1, val2, mode, memos, kIsMap);
  } else if (isAnyArrayBuffer(val1)) {
    if (!isAnyArrayBuffer(val2)) {
      return false;
    }
    if (mode !== kPartial || val1.byteLength === val2.byteLength) {
      if (!areEqualArrayBuffers(val1, val2)) {
        return false;
      }
    } else if (!isPartialUint8Array(new Uint8Array(val1), new Uint8Array(val2))) {
      return false;
    }
  } else if (isError(val1)) {
    // Do not compare the stack as it might differ even though the error itself
    // is otherwise identical.
    if (!isError(val2) ||
        !isEnumerableOrIdentical(val1, val2, 'message', mode, memos) ||
        !isEnumerableOrIdentical(val1, val2, 'name', mode, memos) ||
        !isEnumerableOrIdentical(val1, val2, 'cause', mode, memos) ||
        !isEnumerableOrIdentical(val1, val2, 'errors', mode, memos)) {
      return false;
    }
    const hasOwnVal2Cause = hasOwn(val2, 'cause');
    if ((hasOwnVal2Cause !== hasOwn(val1, 'cause') && (mode !== kPartial || hasOwnVal2Cause))) {
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
             ErrorIsError(val2) ||
             val2 instanceof Error) {
    return false;
  } else if (isURL(val1)) {
    if (!isURL(val2) || val1.href !== val2.href) {
      return false;
    }
  } else if (isKeyObject(val1)) {
    if (!isKeyObject(val2) || !val1.equals(val2)) {
      return false;
    }
  } else if (isCryptoKey(val1)) {
    kKeyObject ??= require('internal/crypto/util').kKeyObject;
    if (!isCryptoKey(val2) ||
      val1.extractable !== val2.extractable ||
      !innerDeepEqual(val1.algorithm, val2.algorithm, mode, memos) ||
      !innerDeepEqual(val1.usages, val2.usages, mode, memos) ||
      !innerDeepEqual(val1[kKeyObject], val2[kKeyObject], mode, memos)
    ) {
      return false;
    }
  } else if (isWeakMap(val1) || isWeakSet(val1)) {
    return false;
  }

  return keyCheck(val1, val2, mode, memos, kNoIterator);
}

function getEnumerables(val, keys) {
  return ArrayPrototypeFilter(keys, (key) => hasEnumerable(val, key));
}

function partialSymbolEquiv(val1, val2, keys2) {
  const symbolKeys = getOwnSymbols(val2);
  if (symbolKeys.length !== 0) {
    for (const key of symbolKeys) {
      if (hasEnumerable(val2, key)) {
        if (!hasEnumerable(val1, key)) {
          return false;
        }
        ArrayPrototypePush(keys2, key);
      }
    }
  }
  return true;
}

function keyCheck(val1, val2, mode, memos, iterationType, keys2) {
  // For all remaining Object pairs, including Array, objects and Maps,
  // equivalence is determined by having:
  // a) The same number of owned enumerable properties
  // b) The same set of keys/indexes (although not necessarily the same order)
  // c) Equivalent values for every corresponding key/index
  // d) For Sets and Maps, equal contents
  // Note: this accounts for both named and indexed properties on Arrays.
  const isArrayLikeObject = keys2 !== undefined;

  if (keys2 === undefined) {
    keys2 = ObjectKeys(val2);
  }
  let keys1;

  if (!isArrayLikeObject) {
    // The pair must have the same number of owned properties.
    if (mode === kPartial) {
      if (!partialSymbolEquiv(val1, val2, keys2)) {
        return false;
      }
    } else if (keys2.length !== (keys1 = ObjectKeys(val1)).length) {
      return false;
    } else if (mode === kStrict) {
      const symbolKeysA = getOwnSymbols(val1);
      if (symbolKeysA.length !== 0) {
        let count = 0;
        for (const key of symbolKeysA) {
          if (hasEnumerable(val1, key)) {
            if (!hasEnumerable(val2, key)) {
              return false;
            }
            ArrayPrototypePush(keys2, key);
            count++;
          } else if (hasEnumerable(val2, key)) {
            return false;
          }
        }
        const symbolKeysB = getOwnSymbols(val2);
        if (symbolKeysA.length !== symbolKeysB.length &&
            getEnumerables(val2, symbolKeysB).length !== count) {
          return false;
        }
      } else {
        const symbolKeysB = getOwnSymbols(val2);
        if (symbolKeysB.length !== 0 &&
            getEnumerables(val2, symbolKeysB).length !== 0) {
          return false;
        }
      }
    }
  }

  if (keys2.length === 0 &&
      (iterationType === kNoIterator ||
        (iterationType === kIsArray && val2.length === 0) ||
        val2.size === 0)) {
    return true;
  }

  if (memos === null) {
    return objEquiv(val1, val2, mode, keys1, keys2, memos, iterationType);
  }
  return handleCycles(val1, val2, mode, keys1, keys2, memos, iterationType);
}

function handleCycles(val1, val2, mode, keys1, keys2, memos, iterationType) {
  // Use memos to handle cycles.
  if (memos === undefined) {
    memos = {
      set: undefined,
      a: val1,
      b: val2,
      c: undefined,
      d: undefined,
      deep: false,
    };
    return objEquiv(val1, val2, mode, keys1, keys2, memos, iterationType);
  }

  if (memos.set === undefined) {
    if (memos.deep === false) {
      if (memos.a === val1) {
        return memos.b === val2;
      }
      if (memos.b === val2) {
        return false;
      }
      memos.c = val1;
      memos.d = val2;
      memos.deep = true;
      const result = objEquiv(val1, val2, mode, keys1, keys2, memos, iterationType);
      memos.deep = false;
      return result;
    }
    memos.set = new SafeSet();
    memos.set.add(memos.a);
    memos.set.add(memos.b);
    memos.set.add(memos.c);
    memos.set.add(memos.d);
  }

  const { set } = memos;

  const originalSize = set.size;
  set.add(val1);
  set.add(val2);
  if (originalSize !== set.size - 2) {
    return originalSize === set.size;
  }

  const areEq = objEquiv(val1, val2, mode, keys1, keys2, memos, iterationType);

  set.delete(val1);
  set.delete(val2);

  return areEq;
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
      // Check for NaN
      // eslint-disable-next-line no-self-compare
      if (prim !== prim) {
        return false;
      }
  }
  return true;
}

function setMightHaveLoosePrim(a, b, prim) {
  const altValue = findLooseMatchingPrimitives(prim);
  if (altValue != null)
    return altValue;

  return !b.has(altValue) && a.has(altValue);
}

function mapMightHaveLoosePrim(a, b, prim, item2, memo) {
  const altValue = findLooseMatchingPrimitives(prim);
  if (altValue != null) {
    return altValue;
  }
  const item1 = a.get(altValue);
  if ((item1 === undefined && !a.has(altValue)) ||
      !innerDeepEqual(item1, item2, kLoose, memo)) {
    return false;
  }
  return !b.has(altValue) && innerDeepEqual(item1, item2, kLoose, memo);
}

function partialObjectSetEquiv(array, a, b, mode, memo) {
  let aPos = 0;
  let direction = 1;
  let start = 0;
  let end = array.length - 1;
  for (const val1 of a) {
    aPos++;
    if (!b.has(val1)) {
      let innerStart = start;
      if (direction === 1) {
        if (innerDeepEqual(val1, array[start], mode, memo)) {
          if (start === end) {
            return true;
          }
          start += 1;
          continue;
        }
        if (start === end) {
          // The last element of set b might match a later element in set a.
          continue;
        }
        direction = -1;
        innerStart += 1;
      }
      let matched = true;
      if (!innerDeepEqual(val1, array[end], mode, memo)) {
        direction = 1;
        matched = arrayHasEqualElement(array, val1, mode, memo, innerDeepEqual, innerStart, end);
      }
      if (matched) {
        if (start === end) {
          return true;
        }
        end -= 1;
      }
    }
    if (a.size - aPos <= end - start) {
      return false;
    }
  }
  return false;
}

function arrayHasEqualElement(array, val1, mode, memo, comparator, start, end) {
  let matched = false;
  for (let i = end - 1; i >= start; i--) {
    if (comparator(val1, array[i], mode, memo)) {
      // Remove the matching element to make sure we do not check that again.
      array.splice(i, 1);
      matched = true;
      break;
    }
  }
  return matched;
}

function setObjectEquiv(array, a, b, mode, memo) {
  let direction = 1;
  let start = 0;
  let end = array.length - 1;
  const comparator = mode !== kLoose ? objectComparisonStart : innerDeepEqual;
  const extraChecks = mode === kLoose || array.length !== a.size;
  for (const val1 of a) {
    if (extraChecks) {
      if (typeof val1 === 'object') {
        if (b.has(val1)) {
          continue;
        }
      } else if (mode !== kLoose || b.has(val1)) {
        continue;
      }
    }

    let innerStart = start;
    if (direction === 1) {
      if (comparator(val1, array[start], mode, memo)) {
        start += 1;
        continue;
      }
      if (start === end) {
        return false;
      }
      direction = -1;
      innerStart += 1;
    }
    if (!comparator(val1, array[end], mode, memo)) {
      direction = 1;
      if (!arrayHasEqualElement(array, val1, mode, memo, comparator, innerStart, end)) {
        return false;
      }
    }
    end -= 1;
  }
  return true;
}

function compareSmallSets(a, b, val, iteratorB, mode, memo) {
  const iteratorA = a.values();
  const firstA = iteratorA.next().value;
  const first = innerDeepEqual(firstA, val, mode, memo);
  if (first) {
    if (b.size === 1) { // Partial mode && a.size === 1 || b.size === 1
      return true;
    }
    const secondA = iteratorA.next().value;
    return b.has(secondA) || innerDeepEqual(secondA, iteratorB.next().value, mode, memo);
  }
  return a.size !== 1 && innerDeepEqual(iteratorA.next().value, val, mode, memo) && (
    b.size === 1 || // Partial mode
    b.has(firstA) || // Primitive or reference equal
    innerDeepEqual(firstA, iteratorB.next().value, mode, memo)
  );
}

function setEquiv(a, b, mode, memo) {
  // This is a lazily initiated Set of entries which have to be compared
  // pairwise.
  let array;

  const iteratorB = b.values();
  for (const val of iteratorB) {
    if (!a.has(val)) {
      if ((typeof val !== 'object' || val === null) &&
          (mode !== kLoose || !setMightHaveLoosePrim(a, b, val))) {
        return false;
      }

      if (array === undefined) {
        if (a.size < 3) {
          return compareSmallSets(a, b, val, iteratorB, mode, memo);
        }
        array = [];
      }
      // If the specified value doesn't exist in the second set it's a object
      // (or in loose mode: a non-matching primitive). Find the
      // deep-(mode-)equal element in a set copy to reduce duplicate checks.
      array.push(val);
    }
  }

  if (array === undefined) {
    return true;
  }
  if (mode === kPartial) {
    return partialObjectSetEquiv(array, a, b, mode, memo);
  }
  return setObjectEquiv(array, a, b, mode, memo);
}

function partialObjectMapEquiv(array, a, b, mode, memo) {
  let aPos = 0;
  let direction = 1;
  let start = 0;
  let end = array.length - 1;
  for (const { 0: key1, 1: item1 } of a) {
    aPos++;
    if (typeof key1 === 'object' && key1 !== null) {
      let innerStart = start;
      if (direction === 1) {
        const key2 = array[start];
        if (objectComparisonStart(key1, key2, mode, memo) && innerDeepEqual(item1, b.get(key2), mode, memo)) {
          if (start === end) {
            return true;
          }
          start += 1;
          continue;
        }
        if (start === end) {
          // The last element of map b might match a later element in map a.
          continue;
        }
        direction = -1;
        innerStart += 1;
      }
      let matched = true;
      const key2 = array[end];
      if (!objectComparisonStart(key1, key2, mode, memo) || !innerDeepEqual(item1, b.get(key2), mode, memo)) {
        direction = 1;
        matched = arrayHasEqualMapElement(array, key1, item1, b, mode, memo, objectComparisonStart, innerStart, end);
      }
      if (matched) {
        if (start === end) {
          return true;
        }
        end -= 1;
      }
    }
    if (a.size - aPos <= end - start) {
      return false;
    }
  }
  return false;
}

function arrayHasEqualMapElement(array, key1, item1, b, mode, memo, comparator, start, end) {
  let matched = false;
  for (let i = end - 1; i >= start; i--) {
    const key2 = array[i];
    if (comparator(key1, key2, mode, memo) &&
        innerDeepEqual(item1, b.get(key2), mode, memo)) {
      // Remove the matching element to make sure we do not check that again.
      array.splice(i, 1);
      matched = true;
      break;
    }
  }
  return matched;
}

function mapObjectEquiv(array, a, b, mode, memo) {
  let direction = 1;
  let start = 0;
  let end = array.length - 1;
  const comparator = mode !== kLoose ? objectComparisonStart : innerDeepEqual;
  const extraChecks = mode === kLoose || array.length !== a.size;

  for (const { 0: key1, 1: item1 } of a) {
    if (extraChecks &&
        (typeof key1 !== 'object' || key1 === null) &&
        (mode !== kLoose ||
          (b.has(key1) && innerDeepEqual(item1, b.get(key1), mode, memo)))) { // Mixed mode
      continue;
    }

    let innerStart = start;
    if (direction === 1) {
      const key2 = array[start];
      if (comparator(key1, key2, mode, memo) && innerDeepEqual(item1, b.get(key2), mode, memo)) {
        start += 1;
        continue;
      }
      if (start === end) {
        return false;
      }
      direction = -1;
      innerStart += 1;
    }
    const key2 = array[end];
    if ((!comparator(key1, key2, mode, memo) || !innerDeepEqual(item1, b.get(key2), mode, memo))) {
      direction = 1;
      if (!arrayHasEqualMapElement(array, key1, item1, b, mode, memo, comparator, innerStart, end)) {
        return false;
      }
    }
    end -= 1;
  }
  return true;
}

function mapEquiv(a, b, mode, memo) {
  let array;

  for (const { 0: key2, 1: item2 } of b) {
    if (typeof key2 === 'object' && key2 !== null) {
      if (array === undefined) {
        if (a.size === 1) {
          const { 0: key1, 1: item1 } = a.entries().next().value;
          return innerDeepEqual(key1, key2, mode, memo) &&
                  innerDeepEqual(item1, item2, mode, memo);
        }
        array = [];
      }
      array.push(key2);
    } else {
      // By directly retrieving the value we prevent another b.has(key2) check in
      // almost all possible cases.
      const item1 = a.get(key2);
      if (((item1 === undefined && !a.has(key2)) ||
          !innerDeepEqual(item1, item2, mode, memo))) {
        if (mode !== kLoose)
          return false;
        // Fast path to detect missing string, symbol, undefined and null
        // keys.
        if (!mapMightHaveLoosePrim(a, b, key2, item2, memo))
          return false;
        if (array === undefined) {
          array = [];
        }
        array.push(key2);
      }
    }
  }

  if (array === undefined) {
    return true;
  }

  if (mode === kPartial) {
    return partialObjectMapEquiv(array, a, b, mode, memo);
  }

  return mapObjectEquiv(array, a, b, mode, memo);
}

function partialSparseArrayEquiv(a, b, mode, memos, startA, startB) {
  let aPos = 0;
  const keysA = ObjectKeys(a).slice(startA);
  const keysB = ObjectKeys(b).slice(startB);
  if (keysA.length < keysB.length) {
    return false;
  }
  for (let i = 0; i < keysB.length; i++) {
    const keyB = keysB[i];
    while (!innerDeepEqual(a[keysA[aPos]], b[keyB], mode, memos)) {
      aPos++;
      if (aPos > keysA.length - keysB.length + i) {
        return false;
      }
    }
    aPos++;
  }
  return true;
}

function partialArrayEquiv(a, b, mode, memos) {
  let aPos = 0;
  for (let i = 0; i < b.length; i++) {
    let isSparse = b[i] === undefined && !hasOwn(b, i);
    if (isSparse) {
      return partialSparseArrayEquiv(a, b, mode, memos, aPos, i);
    }
    while (!(isSparse = a[aPos] === undefined && !hasOwn(a, aPos)) &&
           !innerDeepEqual(a[aPos], b[i], mode, memos)) {
      aPos++;
      if (aPos > a.length - b.length + i) {
        return false;
      }
    }
    if (isSparse) {
      return partialSparseArrayEquiv(a, b, mode, memos, aPos, i);
    }
    aPos++;
  }
  return true;
}

function sparseArrayEquiv(a, b, mode, memos, i) {
  // TODO(BridgeAR): Use internal method to only get index properties. The
  // same applies to the partial implementation.
  const keysA = ObjectKeys(a);
  const keysB = ObjectKeys(b);
  if (keysA.length !== keysB.length) {
    return false;
  }
  for (; i < keysB.length; i++) {
    const key = keysB[i];
    if ((a[key] === undefined && !hasOwn(a, key)) || !innerDeepEqual(a[key], b[key], mode, memos)) {
      return false;
    }
  }
  return true;
}

function objEquiv(a, b, mode, keys1, keys2, memos, iterationType) {
  // The pair must have equivalent values for every corresponding key.
  if (keys2.length > 0) {
    let i = 0;
    // Ordered keys
    if (keys1 !== undefined) {
      for (; i < keys2.length; i++) {
        const key = keys2[i];
        if (keys1[i] !== key) {
          break;
        }
        if (!innerDeepEqual(a[key], b[key], mode, memos)) {
          return false;
        }
      }
    }
    // Unordered keys
    for (; i < keys2.length; i++) {
      const key = keys2[i];
      // It is faster to get the whole descriptor and to check it's enumerable
      // property in V8 13.0 compared to calling Object.propertyIsEnumerable()
      // and accessing the property regularly.
      const descriptor = ObjectGetOwnPropertyDescriptor(a, key);
      if (!descriptor?.enumerable ||
          !innerDeepEqual(descriptor.value !== undefined ? descriptor.value : a[key], b[key], mode, memos)) {
        return false;
      }
    }
  }

  if (iterationType === kIsArray) {
    if (mode === kPartial) {
      return partialArrayEquiv(a, b, mode, memos);
    }
    for (let i = 0; i < a.length; i++) {
      if (b[i] === undefined) {
        if (!hasOwn(b, i))
          return sparseArrayEquiv(a, b, mode, memos, i);
        if (a[i] !== undefined || !hasOwn(a, i))
          return false;
      } else if (a[i] === undefined || !innerDeepEqual(a[i], b[i], mode, memos)) {
        return false;
      }
    }
  } else if (iterationType === kIsSet) {
    if (!setEquiv(a, b, mode, memos)) {
      return false;
    }
  } else if (iterationType === kIsMap) {
    if (!mapEquiv(a, b, mode, memos)) {
      return false;
    }
  }

  return true;
}

// Only handle cycles when they are detected.
// eslint-disable-next-line func-style
let detectCycles = function(val1, val2, mode) {
  try {
    return innerDeepEqual(val1, val2, mode, null);
  } catch {
    detectCycles = innerDeepEqual;
    return innerDeepEqual(val1, val2, mode, undefined);
  }
};

module.exports = {
  isDeepEqual(val1, val2) {
    return detectCycles(val1, val2, kLoose);
  },
  isDeepStrictEqual(val1, val2) {
    return detectCycles(val1, val2, kStrict);
  },
  isPartialStrictEqual(val1, val2) {
    return detectCycles(val1, val2, kPartial);
  },
};
