// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

// array.js has to come before typedarray.js for this to work
var ArrayToString = utils.ImportNow("ArrayToString");
var ArrayValues;
var GetIterator;
var GetMethod;
var GlobalArray = global.Array;
var GlobalArrayBuffer = global.ArrayBuffer;
var GlobalArrayBufferPrototype = GlobalArrayBuffer.prototype;
var GlobalObject = global.Object;
var InnerArrayCopyWithin;
var InnerArrayEvery;
var InnerArrayFill;
var InnerArrayFilter;
var InnerArrayFind;
var InnerArrayFindIndex;
var InnerArrayForEach;
var InnerArrayJoin;
var InnerArrayReduce;
var InnerArrayReduceRight;
var InnerArraySome;
var InnerArraySort;
var InnerArrayToLocaleString;
var InternalArray = utils.InternalArray;
var MaxSimple;
var MinSimple;
var PackedArrayReverse;
var SpeciesConstructor;
var ToPositiveInteger;
var ToIndex;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var speciesSymbol = utils.ImportNow("species_symbol");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

macro TYPED_ARRAYS(FUNCTION)
// arrayIds below should be synchronized with Runtime_TypedArrayInitialize.
FUNCTION(1, Uint8Array, 1)
FUNCTION(2, Int8Array, 1)
FUNCTION(3, Uint16Array, 2)
FUNCTION(4, Int16Array, 2)
FUNCTION(5, Uint32Array, 4)
FUNCTION(6, Int32Array, 4)
FUNCTION(7, Float32Array, 4)
FUNCTION(8, Float64Array, 8)
FUNCTION(9, Uint8ClampedArray, 1)
endmacro

macro DECLARE_GLOBALS(INDEX, NAME, SIZE)
var GlobalNAME = global.NAME;
endmacro

TYPED_ARRAYS(DECLARE_GLOBALS)

var GlobalTypedArray = %object_get_prototype_of(GlobalUint8Array);

utils.Import(function(from) {
  ArrayValues = from.ArrayValues;
  GetIterator = from.GetIterator;
  GetMethod = from.GetMethod;
  InnerArrayCopyWithin = from.InnerArrayCopyWithin;
  InnerArrayEvery = from.InnerArrayEvery;
  InnerArrayFill = from.InnerArrayFill;
  InnerArrayFilter = from.InnerArrayFilter;
  InnerArrayFind = from.InnerArrayFind;
  InnerArrayFindIndex = from.InnerArrayFindIndex;
  InnerArrayForEach = from.InnerArrayForEach;
  InnerArrayJoin = from.InnerArrayJoin;
  InnerArrayReduce = from.InnerArrayReduce;
  InnerArrayReduceRight = from.InnerArrayReduceRight;
  InnerArraySome = from.InnerArraySome;
  InnerArraySort = from.InnerArraySort;
  InnerArrayToLocaleString = from.InnerArrayToLocaleString;
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
  PackedArrayReverse = from.PackedArrayReverse;
  SpeciesConstructor = from.SpeciesConstructor;
  ToPositiveInteger = from.ToPositiveInteger;
  ToIndex = from.ToIndex;
});

// --------------- Typed Arrays ---------------------

function TypedArrayDefaultConstructor(typedArray) {
  switch (%_ClassOf(typedArray)) {
macro TYPED_ARRAY_CONSTRUCTOR_CASE(ARRAY_ID, NAME, ELEMENT_SIZE)
    case "NAME":
      return GlobalNAME;
endmacro
TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTOR_CASE)
  }
  // The TypeError should not be generated since all callers should
  // have already called ValidateTypedArray.
  throw %make_type_error(kIncompatibleMethodReceiver,
                      "TypedArrayDefaultConstructor", this);
}

function TypedArrayCreate(constructor, arg0, arg1, arg2) {
  if (IS_UNDEFINED(arg1)) {
    var newTypedArray = new constructor(arg0);
  } else {
    var newTypedArray = new constructor(arg0, arg1, arg2);
  }
  if (!IS_TYPEDARRAY(newTypedArray)) throw %make_type_error(kNotTypedArray);
  // TODO(littledan): Check for being detached, here and elsewhere
  // All callers where the first argument is a Number have no additional
  // arguments.
  if (IS_NUMBER(arg0) && %_TypedArrayGetLength(newTypedArray) < arg0) {
    throw %make_type_error(kTypedArrayTooShort);
  }
  return newTypedArray;
}

function TypedArraySpeciesCreate(exemplar, arg0, arg1, arg2, conservative) {
  var defaultConstructor = TypedArrayDefaultConstructor(exemplar);
  var constructor = SpeciesConstructor(exemplar, defaultConstructor,
                                       conservative);
  return TypedArrayCreate(constructor, arg0, arg1, arg2);
}

macro TYPED_ARRAY_CONSTRUCTOR(ARRAY_ID, NAME, ELEMENT_SIZE)
function NAMEConstructByArrayBuffer(obj, buffer, byteOffset, length) {
  if (!IS_UNDEFINED(byteOffset)) {
    byteOffset = ToIndex(byteOffset, kInvalidTypedArrayLength);
  }
  if (!IS_UNDEFINED(length)) {
    length = ToIndex(length, kInvalidTypedArrayLength);
  }
  if (length > %_MaxSmi()) {
    // Note: this is not per spec, but rather a constraint of our current
    // representation (which uses smi's).
    throw %make_range_error(kInvalidTypedArrayLength);
  }

  var bufferByteLength = %_ArrayBufferGetByteLength(buffer);
  var offset;
  if (IS_UNDEFINED(byteOffset)) {
    offset = 0;
  } else {
    offset = byteOffset;

    if (offset % ELEMENT_SIZE !== 0) {
      throw %make_range_error(kInvalidTypedArrayAlignment,
                           "start offset", "NAME", ELEMENT_SIZE);
    }
  }

  var newByteLength;
  if (IS_UNDEFINED(length)) {
    if (bufferByteLength % ELEMENT_SIZE !== 0) {
      throw %make_range_error(kInvalidTypedArrayAlignment,
                           "byte length", "NAME", ELEMENT_SIZE);
    }
    newByteLength = bufferByteLength - offset;
    if (newByteLength < 0) {
      throw %make_range_error(kInvalidTypedArrayAlignment,
                           "byte length", "NAME", ELEMENT_SIZE);
    }
  } else {
    newByteLength = length * ELEMENT_SIZE;
    if (offset + newByteLength > bufferByteLength) {
      throw %make_range_error(kInvalidTypedArrayLength);
    }
  }
  %_TypedArrayInitialize(obj, ARRAY_ID, buffer, offset, newByteLength, true);
}

function NAMEConstructByLength(obj, length) {
  var l = IS_UNDEFINED(length) ?
    0 : ToIndex(length, kInvalidTypedArrayLength);
  if (length > %_MaxSmi()) {
    // Note: this is not per spec, but rather a constraint of our current
    // representation (which uses smi's).
    throw %make_range_error(kInvalidTypedArrayLength);
  }
  var byteLength = l * ELEMENT_SIZE;
  if (byteLength > %_TypedArrayMaxSizeInHeap()) {
    var buffer = new GlobalArrayBuffer(byteLength);
    %_TypedArrayInitialize(obj, ARRAY_ID, buffer, 0, byteLength, true);
  } else {
    %_TypedArrayInitialize(obj, ARRAY_ID, null, 0, byteLength, true);
  }
}

function NAMEConstructByArrayLike(obj, arrayLike, length) {
  var l = ToPositiveInteger(length, kInvalidTypedArrayLength);

  if (l > %_MaxSmi()) {
    throw %make_range_error(kInvalidTypedArrayLength);
  }
  var initialized = false;
  var byteLength = l * ELEMENT_SIZE;
  if (byteLength <= %_TypedArrayMaxSizeInHeap()) {
    %_TypedArrayInitialize(obj, ARRAY_ID, null, 0, byteLength, false);
  } else {
    initialized =
        %TypedArrayInitializeFromArrayLike(obj, ARRAY_ID, arrayLike, l);
  }
  if (!initialized) {
    for (var i = 0; i < l; i++) {
      // It is crucial that we let any execptions from arrayLike[i]
      // propagate outside the function.
      obj[i] = arrayLike[i];
    }
  }
}

function NAMEConstructByIterable(obj, iterable, iteratorFn) {
  var list = new InternalArray();
  // Reading the Symbol.iterator property of iterable twice would be
  // observable with getters, so instead, we call the function which
  // was already looked up, and wrap it in another iterable. The
  // __proto__ of the new iterable is set to null to avoid any chance
  // of modifications to Object.prototype being observable here.
  var iterator = %_Call(iteratorFn, iterable);
  var newIterable = {
    __proto__: null
  };
  // TODO(littledan): Computed properties don't work yet in nosnap.
  // Rephrase when they do.
  newIterable[iteratorSymbol] = function() { return iterator; }
  for (var value of newIterable) {
    list.push(value);
  }
  NAMEConstructByArrayLike(obj, list, list.length);
}

// ES#sec-typedarray-typedarray TypedArray ( typedArray )
function NAMEConstructByTypedArray(obj, typedArray) {
  // TODO(littledan): Throw on detached typedArray
  var srcData = %TypedArrayGetBuffer(typedArray);
  var length = %_TypedArrayGetLength(typedArray);
  var byteLength = %_ArrayBufferViewGetByteLength(typedArray);
  var newByteLength = length * ELEMENT_SIZE;
  NAMEConstructByArrayLike(obj, typedArray, length);
  var bufferConstructor = SpeciesConstructor(srcData, GlobalArrayBuffer);
  var prototype = bufferConstructor.prototype;
  // TODO(littledan): Use the right prototype based on bufferConstructor's realm
  if (IS_RECEIVER(prototype) && prototype !== GlobalArrayBufferPrototype) {
    %InternalSetPrototype(%TypedArrayGetBuffer(obj), prototype);
  }
}

function NAMEConstructor(arg1, arg2, arg3) {
  if (!IS_UNDEFINED(new.target)) {
    if (IS_ARRAYBUFFER(arg1) || IS_SHAREDARRAYBUFFER(arg1)) {
      NAMEConstructByArrayBuffer(this, arg1, arg2, arg3);
    } else if (IS_TYPEDARRAY(arg1)) {
      NAMEConstructByTypedArray(this, arg1);
    } else if (IS_RECEIVER(arg1)) {
      var iteratorFn = arg1[iteratorSymbol];
      if (IS_UNDEFINED(iteratorFn) || iteratorFn === ArrayValues) {
        NAMEConstructByArrayLike(this, arg1, arg1.length);
      } else {
        NAMEConstructByIterable(this, arg1, iteratorFn);
      }
    } else {
      NAMEConstructByLength(this, arg1);
    }
  } else {
    throw %make_type_error(kConstructorNotFunction, "NAME")
  }
}

function NAMESubArray(begin, end) {
  var beginInt = TO_INTEGER(begin);
  if (!IS_UNDEFINED(end)) {
    var endInt = TO_INTEGER(end);
    var srcLength = %_TypedArrayGetLength(this);
  } else {
    var srcLength = %_TypedArrayGetLength(this);
    var endInt = srcLength;
  }

  if (beginInt < 0) {
    beginInt = MaxSimple(0, srcLength + beginInt);
  } else {
    beginInt = MinSimple(beginInt, srcLength);
  }

  if (endInt < 0) {
    endInt = MaxSimple(0, srcLength + endInt);
  } else {
    endInt = MinSimple(endInt, srcLength);
  }

  if (endInt < beginInt) {
    endInt = beginInt;
  }

  var newLength = endInt - beginInt;
  var beginByteOffset =
      %_ArrayBufferViewGetByteOffset(this) + beginInt * ELEMENT_SIZE;
  // BUG(v8:4665): For web compatibility, subarray needs to always build an
  // instance of the default constructor.
  // TODO(littledan): Switch to the standard or standardize the fix
  return new GlobalNAME(%TypedArrayGetBuffer(this), beginByteOffset, newLength);
}
endmacro

TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTOR)

function TypedArraySubArray(begin, end) {
  switch (%_ClassOf(this)) {
macro TYPED_ARRAY_SUBARRAY_CASE(ARRAY_ID, NAME, ELEMENT_SIZE)
    case "NAME":
      return %_Call(NAMESubArray, this, begin, end);
endmacro
TYPED_ARRAYS(TYPED_ARRAY_SUBARRAY_CASE)
  }
  throw %make_type_error(kIncompatibleMethodReceiver,
                      "get TypedArray.prototype.subarray", this);
}
%SetForceInlineFlag(TypedArraySubArray);



function TypedArraySetFromArrayLike(target, source, sourceLength, offset) {
  if (offset > 0) {
    for (var i = 0; i < sourceLength; i++) {
      target[offset + i] = source[i];
    }
  }
  else {
    for (var i = 0; i < sourceLength; i++) {
      target[i] = source[i];
    }
  }
}

function TypedArraySetFromOverlappingTypedArray(target, source, offset) {
  var sourceElementSize = source.BYTES_PER_ELEMENT;
  var targetElementSize = target.BYTES_PER_ELEMENT;
  var sourceLength = %_TypedArrayGetLength(source);

  // Copy left part.
  function CopyLeftPart() {
    // First un-mutated byte after the next write
    var targetPtr = target.byteOffset + (offset + 1) * targetElementSize;
    // Next read at sourcePtr. We do not care for memory changing before
    // sourcePtr - we have already copied it.
    var sourcePtr = source.byteOffset;
    for (var leftIndex = 0;
         leftIndex < sourceLength && targetPtr <= sourcePtr;
         leftIndex++) {
      target[offset + leftIndex] = source[leftIndex];
      targetPtr += targetElementSize;
      sourcePtr += sourceElementSize;
    }
    return leftIndex;
  }
  var leftIndex = CopyLeftPart();

  // Copy right part;
  function CopyRightPart() {
    // First unmutated byte before the next write
    var targetPtr =
      target.byteOffset + (offset + sourceLength - 1) * targetElementSize;
    // Next read before sourcePtr. We do not care for memory changing after
    // sourcePtr - we have already copied it.
    var sourcePtr =
      source.byteOffset + sourceLength * sourceElementSize;
    for(var rightIndex = sourceLength - 1;
        rightIndex >= leftIndex && targetPtr >= sourcePtr;
        rightIndex--) {
      target[offset + rightIndex] = source[rightIndex];
      targetPtr -= targetElementSize;
      sourcePtr -= sourceElementSize;
    }
    return rightIndex;
  }
  var rightIndex = CopyRightPart();

  var temp = new GlobalArray(rightIndex + 1 - leftIndex);
  for (var i = leftIndex; i <= rightIndex; i++) {
    temp[i - leftIndex] = source[i];
  }
  for (i = leftIndex; i <= rightIndex; i++) {
    target[offset + i] = temp[i - leftIndex];
  }
}

function TypedArraySet(obj, offset) {
  var intOffset = IS_UNDEFINED(offset) ? 0 : TO_INTEGER(offset);
  if (intOffset < 0) throw %make_type_error(kTypedArraySetNegativeOffset);

  if (intOffset > %_MaxSmi()) {
    throw %make_range_error(kTypedArraySetSourceTooLarge);
  }
  switch (%TypedArraySetFastCases(this, obj, intOffset)) {
    // These numbers should be synchronized with runtime.cc.
    case 0: // TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE
      return;
    case 1: // TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING
      TypedArraySetFromOverlappingTypedArray(this, obj, intOffset);
      return;
    case 2: // TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING
      TypedArraySetFromArrayLike(this,
          obj, %_TypedArrayGetLength(obj), intOffset);
      return;
    case 3: // TYPED_ARRAY_SET_NON_TYPED_ARRAY
      var l = obj.length;
      if (IS_UNDEFINED(l)) {
        if (IS_NUMBER(obj)) {
            // For number as a first argument, throw TypeError
            // instead of silently ignoring the call, so that
            // the user knows (s)he did something wrong.
            // (Consistent with Firefox and Blink/WebKit)
            throw %make_type_error(kInvalidArgument);
        }
        return;
      }
      l = TO_LENGTH(l);
      if (intOffset + l > %_TypedArrayGetLength(this)) {
        throw %make_range_error(kTypedArraySetSourceTooLarge);
      }
      TypedArraySetFromArrayLike(this, obj, l, intOffset);
      return;
  }
}
%FunctionSetLength(TypedArraySet, 1);

function TypedArrayGetToStringTag() {
  if (!IS_TYPEDARRAY(this)) return;
  var name = %_ClassOf(this);
  if (IS_UNDEFINED(name)) return;
  return name;
}


function TypedArrayCopyWithin(target, start, end) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  // TODO(littledan): Replace with a memcpy for better performance
  return InnerArrayCopyWithin(target, start, end, this, length);
}
%FunctionSetLength(TypedArrayCopyWithin, 2);


// ES6 draft 05-05-15, section 22.2.3.7
function TypedArrayEvery(f, receiver) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayEvery(f, receiver, this, length);
}
%FunctionSetLength(TypedArrayEvery, 1);


// ES6 draft 08-24-14, section 22.2.3.12
function TypedArrayForEach(f, receiver) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  InnerArrayForEach(f, receiver, this, length);
}
%FunctionSetLength(TypedArrayForEach, 1);


// ES6 draft 04-05-14 section 22.2.3.8
function TypedArrayFill(value, start, end) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFill(value, start, end, this, length);
}
%FunctionSetLength(TypedArrayFill, 1);


// ES6 draft 07-15-13, section 22.2.3.9
function TypedArrayFilter(f, thisArg) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  if (!IS_CALLABLE(f)) throw %make_type_error(kCalledNonCallable, f);
  var result = new InternalArray();
  InnerArrayFilter(f, thisArg, this, length, result);
  var captured = result.length;
  var output = TypedArraySpeciesCreate(this, captured);
  for (var i = 0; i < captured; i++) {
    output[i] = result[i];
  }
  return output;
}
%FunctionSetLength(TypedArrayFilter, 1);


// ES6 draft 07-15-13, section 22.2.3.10
function TypedArrayFind(predicate, thisArg) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFind(predicate, thisArg, this, length);
}
%FunctionSetLength(TypedArrayFind, 1);


// ES6 draft 07-15-13, section 22.2.3.11
function TypedArrayFindIndex(predicate, thisArg) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFindIndex(predicate, thisArg, this, length);
}
%FunctionSetLength(TypedArrayFindIndex, 1);


// ES6 draft 05-18-15, section 22.2.3.21
function TypedArrayReverse() {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return PackedArrayReverse(this, length);
}


function TypedArrayComparefn(x, y) {
  if (x === 0 && x === y) {
    x = 1 / x;
    y = 1 / y;
  }
  if (x < y) {
    return -1;
  } else if (x > y) {
    return 1;
  } else if (NUMBER_IS_NAN(x) && NUMBER_IS_NAN(y)) {
    return NUMBER_IS_NAN(y) ? 0 : 1;
  } else if (NUMBER_IS_NAN(x)) {
    return 1;
  }
  return 0;
}


// ES6 draft 05-18-15, section 22.2.3.25
function TypedArraySort(comparefn) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  if (IS_UNDEFINED(comparefn)) {
    comparefn = TypedArrayComparefn;
  }

  return InnerArraySort(this, length, comparefn);
}


// ES6 section 22.2.3.13
function TypedArrayIndexOf(element, index) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  if (length === 0) return -1;
  if (!IS_NUMBER(element)) return -1;
  var n = TO_INTEGER(index);

  var k;
  if (n === 0) {
    k = 0;
  } else if (n > 0) {
    k = n;
  } else {
    k = length + n;
    if (k < 0) {
      k = 0;
    }
  }

  while (k < length) {
    var elementK = this[k];
    if (element === elementK) {
      return k;
    }
    ++k;
  }
  return -1;
}
%FunctionSetLength(TypedArrayIndexOf, 1);


// ES6 section 22.2.3.16
function TypedArrayLastIndexOf(element, index) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  if (length === 0) return -1;
  if (!IS_NUMBER(element)) return -1;
  var n;
  if (arguments.length < 2) {
    n = length - 1;
  } else {
    n = TO_INTEGER(index);
  }

  var k;
  if (n >= 0) {
    if (length <= n) {
      k = length - 1;
    } else if (n === 0) {
      k = 0;
    } else {
      k = n;
    }
  } else {
    k = length + n;
  }

  while (k >= 0) {
    var elementK = this[k];
    if (element === elementK) {
      return k;
    }
    --k;
  }
  return -1;
}
%FunctionSetLength(TypedArrayLastIndexOf, 1);


// ES6 draft 07-15-13, section 22.2.3.18
function TypedArrayMap(f, thisArg) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  var result = TypedArraySpeciesCreate(this, length);
  if (!IS_CALLABLE(f)) throw %make_type_error(kCalledNonCallable, f);
  for (var i = 0; i < length; i++) {
    var element = this[i];
    result[i] = %_Call(f, thisArg, element, i, this);
  }
  return result;
}
%FunctionSetLength(TypedArrayMap, 1);


// ES6 draft 05-05-15, section 22.2.3.24
function TypedArraySome(f, receiver) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArraySome(f, receiver, this, length);
}
%FunctionSetLength(TypedArraySome, 1);


// ES6 section 22.2.3.27
function TypedArrayToLocaleString() {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayToLocaleString(this, length);
}


// ES6 section 22.2.3.14
function TypedArrayJoin(separator) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayJoin(separator, this, length);
}


// ES6 draft 07-15-13, section 22.2.3.19
function TypedArrayReduce(callback, current) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  return InnerArrayReduce(callback, current, this, length,
                          arguments.length);
}
%FunctionSetLength(TypedArrayReduce, 1);


// ES6 draft 07-15-13, section 22.2.3.19
function TypedArrayReduceRight(callback, current) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  return InnerArrayReduceRight(callback, current, this, length,
                               arguments.length);
}
%FunctionSetLength(TypedArrayReduceRight, 1);


function TypedArraySlice(start, end) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);
  var len = %_TypedArrayGetLength(this);

  var relativeStart = TO_INTEGER(start);

  var k;
  if (relativeStart < 0) {
    k = MaxSimple(len + relativeStart, 0);
  } else {
    k = MinSimple(relativeStart, len);
  }

  var relativeEnd;
  if (IS_UNDEFINED(end)) {
    relativeEnd = len;
  } else {
    relativeEnd = TO_INTEGER(end);
  }

  var final;
  if (relativeEnd < 0) {
    final = MaxSimple(len + relativeEnd, 0);
  } else {
    final = MinSimple(relativeEnd, len);
  }

  var count = MaxSimple(final - k, 0);
  var array = TypedArraySpeciesCreate(this, count);
  // The code below is the 'then' branch; the 'else' branch species
  // a memcpy. Because V8 doesn't canonicalize NaN, the difference is
  // unobservable.
  var n = 0;
  while (k < final) {
    var kValue = this[k];
    array[n] = kValue;
    k++;
    n++;
  }
  return array;
}


// ES2016 draft, section 22.2.3.14
function TypedArrayIncludes(searchElement, fromIndex) {
  if (!IS_TYPEDARRAY(this)) throw %make_type_error(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  if (length === 0) return false;
  var n = TO_INTEGER(fromIndex);

  var k;
  if (n >= 0) {
    k = n;
  } else {
    k = length + n;
    if (k < 0) {
      k = 0;
    }
  }

  while (k < length) {
    var elementK = this[k];
    if (%SameValueZero(searchElement, elementK)) {
      return true;
    }

    ++k;
  }

  return false;
}
%FunctionSetLength(TypedArrayIncludes, 1);


// ES6 draft 08-24-14, section 22.2.2.2
function TypedArrayOf() {
  var length = arguments.length;
  var array = TypedArrayCreate(this, length);
  for (var i = 0; i < length; i++) {
    array[i] = arguments[i];
  }
  return array;
}


// ES#sec-iterabletoarraylike Runtime Semantics: IterableToArrayLike( items )
function IterableToArrayLike(items) {
  var iterable = GetMethod(items, iteratorSymbol);
  if (!IS_UNDEFINED(iterable)) {
    var internal_array = new InternalArray();
    var i = 0;
    for (var value of
         { [iteratorSymbol]() { return GetIterator(items, iterable) } }) {
      internal_array[i] = value;
      i++;
    }
    var array = [];
    %MoveArrayContents(internal_array, array);
    return array;
  }
  return TO_OBJECT(items);
}


// ES#sec-%typedarray%.from
// %TypedArray%.from ( source [ , mapfn [ , thisArg ] ] )
function TypedArrayFrom(source, mapfn, thisArg) {
  if (!%IsConstructor(this)) throw %make_type_error(kNotConstructor, this);
  var mapping;
  if (!IS_UNDEFINED(mapfn)) {
    if (!IS_CALLABLE(mapfn)) throw %make_type_error(kCalledNonCallable, this);
    mapping = true;
  } else {
    mapping = false;
  }
  var arrayLike = IterableToArrayLike(source);
  var length = TO_LENGTH(arrayLike.length);
  var targetObject = TypedArrayCreate(this, length);
  var value, mappedValue;
  for (var i = 0; i < length; i++) {
    value = arrayLike[i];
    if (mapping) {
      mappedValue = %_Call(mapfn, thisArg, value, i);
    } else {
      mappedValue = value;
    }
    targetObject[i] = mappedValue;
  }
  return targetObject;
}
%FunctionSetLength(TypedArrayFrom, 1);

// TODO(bmeurer): Migrate this to a proper builtin.
function TypedArrayConstructor() {
  if (IS_UNDEFINED(new.target)) {
    throw %make_type_error(kConstructorNonCallable, "TypedArray");
  }
  if (new.target === GlobalTypedArray) {
    throw %make_type_error(kConstructAbstractClass, "TypedArray");
  }
}

function TypedArraySpecies() {
  return this;
}

// -------------------------------------------------------------------

%SetCode(GlobalTypedArray, TypedArrayConstructor);
utils.InstallFunctions(GlobalTypedArray, DONT_ENUM, [
  "from", TypedArrayFrom,
  "of", TypedArrayOf
]);
utils.InstallGetter(GlobalTypedArray, speciesSymbol, TypedArraySpecies);
utils.InstallGetter(GlobalTypedArray.prototype, toStringTagSymbol,
                    TypedArrayGetToStringTag);
utils.InstallFunctions(GlobalTypedArray.prototype, DONT_ENUM, [
  "subarray", TypedArraySubArray,
  "set", TypedArraySet,
  "copyWithin", TypedArrayCopyWithin,
  "every", TypedArrayEvery,
  "fill", TypedArrayFill,
  "filter", TypedArrayFilter,
  "find", TypedArrayFind,
  "findIndex", TypedArrayFindIndex,
  "includes", TypedArrayIncludes,
  "indexOf", TypedArrayIndexOf,
  "join", TypedArrayJoin,
  "lastIndexOf", TypedArrayLastIndexOf,
  "forEach", TypedArrayForEach,
  "map", TypedArrayMap,
  "reduce", TypedArrayReduce,
  "reduceRight", TypedArrayReduceRight,
  "reverse", TypedArrayReverse,
  "slice", TypedArraySlice,
  "some", TypedArraySome,
  "sort", TypedArraySort,
  "toLocaleString", TypedArrayToLocaleString
]);

%AddNamedProperty(GlobalTypedArray.prototype, "toString", ArrayToString,
                  DONT_ENUM);


macro SETUP_TYPED_ARRAY(ARRAY_ID, NAME, ELEMENT_SIZE)
  %SetCode(GlobalNAME, NAMEConstructor);
  %FunctionSetPrototype(GlobalNAME, new GlobalObject());
  %InternalSetPrototype(GlobalNAME, GlobalTypedArray);
  %InternalSetPrototype(GlobalNAME.prototype, GlobalTypedArray.prototype);

  %AddNamedProperty(GlobalNAME, "BYTES_PER_ELEMENT", ELEMENT_SIZE,
                    READ_ONLY | DONT_ENUM | DONT_DELETE);

  %AddNamedProperty(GlobalNAME.prototype,
                    "constructor", global.NAME, DONT_ENUM);
  %AddNamedProperty(GlobalNAME.prototype,
                    "BYTES_PER_ELEMENT", ELEMENT_SIZE,
                    READ_ONLY | DONT_ENUM | DONT_DELETE);
endmacro

TYPED_ARRAYS(SETUP_TYPED_ARRAY)

})
