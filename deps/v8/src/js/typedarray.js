// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var ArrayFrom;
var ArrayToString;
var ArrayValues;
var GlobalArray = global.Array;
var GlobalArrayBuffer = global.ArrayBuffer;
var GlobalDataView = global.DataView;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var InnerArrayCopyWithin;
var InnerArrayEvery;
var InnerArrayFill;
var InnerArrayFilter;
var InnerArrayFind;
var InnerArrayFindIndex;
var InnerArrayForEach;
var InnerArrayIncludes;
var InnerArrayIndexOf;
var InnerArrayJoin;
var InnerArrayLastIndexOf;
var InnerArrayReduce;
var InnerArrayReduceRight;
var InnerArraySome;
var InnerArraySort;
var InnerArrayToLocaleString;
var InternalArray = utils.InternalArray;
var IsNaN;
var MakeRangeError;
var MakeTypeError;
var MaxSimple;
var MinSimple;
var PackedArrayReverse;
var SpeciesConstructor;
var ToPositiveInteger;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
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

utils.Import(function(from) {
  ArrayFrom = from.ArrayFrom;
  ArrayToString = from.ArrayToString;
  ArrayValues = from.ArrayValues;
  InnerArrayCopyWithin = from.InnerArrayCopyWithin;
  InnerArrayEvery = from.InnerArrayEvery;
  InnerArrayFill = from.InnerArrayFill;
  InnerArrayFilter = from.InnerArrayFilter;
  InnerArrayFind = from.InnerArrayFind;
  InnerArrayFindIndex = from.InnerArrayFindIndex;
  InnerArrayForEach = from.InnerArrayForEach;
  InnerArrayIncludes = from.InnerArrayIncludes;
  InnerArrayIndexOf = from.InnerArrayIndexOf;
  InnerArrayJoin = from.InnerArrayJoin;
  InnerArrayLastIndexOf = from.InnerArrayLastIndexOf;
  InnerArrayReduce = from.InnerArrayReduce;
  InnerArrayReduceRight = from.InnerArrayReduceRight;
  InnerArraySome = from.InnerArraySome;
  InnerArraySort = from.InnerArraySort;
  InnerArrayToLocaleString = from.InnerArrayToLocaleString;
  IsNaN = from.IsNaN;
  MakeRangeError = from.MakeRangeError;
  MakeTypeError = from.MakeTypeError;
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
  PackedArrayReverse = from.PackedArrayReverse;
  SpeciesConstructor = from.SpeciesConstructor;
  ToPositiveInteger = from.ToPositiveInteger;
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
  throw MakeTypeError(kIncompatibleMethodReceiver,
                      "TypedArrayDefaultConstructor", this);
}

function TypedArrayCreate(constructor, arg0, arg1, arg2) {
  if (IS_UNDEFINED(arg1)) {
    var newTypedArray = new constructor(arg0);
  } else {
    var newTypedArray = new constructor(arg0, arg1, arg2);
  }
  if (!%_IsTypedArray(newTypedArray)) throw MakeTypeError(kNotTypedArray);
  // TODO(littledan): Check for being detached, here and elsewhere
  // All callers where the first argument is a Number have no additional
  // arguments.
  if (IS_NUMBER(arg0) && %_TypedArrayGetLength(newTypedArray) < arg0) {
    throw MakeTypeError(kTypedArrayTooShort);
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
    byteOffset = ToPositiveInteger(byteOffset, kInvalidTypedArrayLength);
  }
  if (!IS_UNDEFINED(length)) {
    length = ToPositiveInteger(length, kInvalidTypedArrayLength);
  }

  var bufferByteLength = %_ArrayBufferGetByteLength(buffer);
  var offset;
  if (IS_UNDEFINED(byteOffset)) {
    offset = 0;
  } else {
    offset = byteOffset;

    if (offset % ELEMENT_SIZE !== 0) {
      throw MakeRangeError(kInvalidTypedArrayAlignment,
                           "start offset", "NAME", ELEMENT_SIZE);
    }
    if (offset > bufferByteLength) {
      throw MakeRangeError(kInvalidTypedArrayOffset);
    }
  }

  var newByteLength;
  var newLength;
  if (IS_UNDEFINED(length)) {
    if (bufferByteLength % ELEMENT_SIZE !== 0) {
      throw MakeRangeError(kInvalidTypedArrayAlignment,
                           "byte length", "NAME", ELEMENT_SIZE);
    }
    newByteLength = bufferByteLength - offset;
    newLength = newByteLength / ELEMENT_SIZE;
  } else {
    var newLength = length;
    newByteLength = newLength * ELEMENT_SIZE;
  }
  if ((offset + newByteLength > bufferByteLength)
      || (newLength > %_MaxSmi())) {
    throw MakeRangeError(kInvalidTypedArrayLength);
  }
  %_TypedArrayInitialize(obj, ARRAY_ID, buffer, offset, newByteLength, true);
}

function NAMEConstructByLength(obj, length) {
  var l = IS_UNDEFINED(length) ?
    0 : ToPositiveInteger(length, kInvalidTypedArrayLength);
  if (l > %_MaxSmi()) {
    throw MakeRangeError(kInvalidTypedArrayLength);
  }
  var byteLength = l * ELEMENT_SIZE;
  if (byteLength > %_TypedArrayMaxSizeInHeap()) {
    var buffer = new GlobalArrayBuffer(byteLength);
    %_TypedArrayInitialize(obj, ARRAY_ID, buffer, 0, byteLength, true);
  } else {
    %_TypedArrayInitialize(obj, ARRAY_ID, null, 0, byteLength, true);
  }
}

function NAMEConstructByArrayLike(obj, arrayLike) {
  var length = arrayLike.length;
  var l = ToPositiveInteger(length, kInvalidTypedArrayLength);

  if (l > %_MaxSmi()) {
    throw MakeRangeError(kInvalidTypedArrayLength);
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
  NAMEConstructByArrayLike(obj, list);
}

function NAMEConstructor(arg1, arg2, arg3) {
  if (!IS_UNDEFINED(new.target)) {
    if (IS_ARRAYBUFFER(arg1) || IS_SHAREDARRAYBUFFER(arg1)) {
      NAMEConstructByArrayBuffer(this, arg1, arg2, arg3);
    } else if (IS_NUMBER(arg1) || IS_STRING(arg1) ||
               IS_BOOLEAN(arg1) || IS_UNDEFINED(arg1)) {
      NAMEConstructByLength(this, arg1);
    } else {
      // TODO(littledan): If arg1 is a TypedArray, follow the constructor
      // path in ES2015 22.2.4.3, and call SpeciesConstructor, in a
      // path that seems to be an optimized version of what's below, but
      // in an observably different way.
      var iteratorFn = arg1[iteratorSymbol];
      if (IS_UNDEFINED(iteratorFn) || iteratorFn === ArrayValues) {
        NAMEConstructByArrayLike(this, arg1);
      } else {
        NAMEConstructByIterable(this, arg1, iteratorFn);
      }
    }
  } else {
    throw MakeTypeError(kConstructorNotFunction, "NAME")
  }
}

// TODO(littledan): Remove this performance workaround BUG(chromium:579905)
function NAME_GetLength() {
  if (!(%_ClassOf(this) === 'NAME')) {
    throw MakeTypeError(kIncompatibleMethodReceiver, "NAME.length", this);
  }
  return %_TypedArrayGetLength(this);
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
  return TypedArraySpeciesCreate(this, %TypedArrayGetBuffer(this),
                                 beginByteOffset, newLength, true);
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
  throw MakeTypeError(kIncompatibleMethodReceiver,
                      "get TypedArray.prototype.subarray", this);
}
%SetForceInlineFlag(TypedArraySubArray);

function TypedArrayGetBuffer() {
  if (!%_IsTypedArray(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "get TypedArray.prototype.buffer", this);
  }
  return %TypedArrayGetBuffer(this);
}
%SetForceInlineFlag(TypedArrayGetBuffer);

function TypedArrayGetByteLength() {
  if (!%_IsTypedArray(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "get TypedArray.prototype.byteLength", this);
  }
  return %_ArrayBufferViewGetByteLength(this);
}
%SetForceInlineFlag(TypedArrayGetByteLength);

function TypedArrayGetByteOffset() {
  if (!%_IsTypedArray(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "get TypedArray.prototype.byteOffset", this);
  }
  return %_ArrayBufferViewGetByteOffset(this);
}
%SetForceInlineFlag(TypedArrayGetByteOffset);

function TypedArrayGetLength() {
  if (!%_IsTypedArray(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "get TypedArray.prototype.length", this);
  }
  return %_TypedArrayGetLength(this);
}
%SetForceInlineFlag(TypedArrayGetLength);



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
  var sourceLength = source.length;

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

  // Copy rigth part;
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
  if (intOffset < 0) throw MakeTypeError(kTypedArraySetNegativeOffset);

  if (intOffset > %_MaxSmi()) {
    throw MakeRangeError(kTypedArraySetSourceTooLarge);
  }
  switch (%TypedArraySetFastCases(this, obj, intOffset)) {
    // These numbers should be synchronized with runtime.cc.
    case 0: // TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE
      return;
    case 1: // TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING
      TypedArraySetFromOverlappingTypedArray(this, obj, intOffset);
      return;
    case 2: // TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING
      TypedArraySetFromArrayLike(this, obj, obj.length, intOffset);
      return;
    case 3: // TYPED_ARRAY_SET_NON_TYPED_ARRAY
      var l = obj.length;
      if (IS_UNDEFINED(l)) {
        if (IS_NUMBER(obj)) {
            // For number as a first argument, throw TypeError
            // instead of silently ignoring the call, so that
            // the user knows (s)he did something wrong.
            // (Consistent with Firefox and Blink/WebKit)
            throw MakeTypeError(kInvalidArgument);
        }
        return;
      }
      l = TO_LENGTH(l);
      if (intOffset + l > this.length) {
        throw MakeRangeError(kTypedArraySetSourceTooLarge);
      }
      TypedArraySetFromArrayLike(this, obj, l, intOffset);
      return;
  }
}

function TypedArrayGetToStringTag() {
  if (!%_IsTypedArray(this)) return;
  var name = %_ClassOf(this);
  if (IS_UNDEFINED(name)) return;
  return name;
}


function TypedArrayCopyWithin(target, start, end) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  // TODO(littledan): Replace with a memcpy for better performance
  return InnerArrayCopyWithin(target, start, end, this, length);
}
%FunctionSetLength(TypedArrayCopyWithin, 2);


// ES6 draft 05-05-15, section 22.2.3.7
function TypedArrayEvery(f, receiver) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayEvery(f, receiver, this, length);
}
%FunctionSetLength(TypedArrayEvery, 1);


// ES6 draft 08-24-14, section 22.2.3.12
function TypedArrayForEach(f, receiver) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  InnerArrayForEach(f, receiver, this, length);
}
%FunctionSetLength(TypedArrayForEach, 1);


// ES6 draft 04-05-14 section 22.2.3.8
function TypedArrayFill(value, start, end) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFill(value, start, end, this, length);
}
%FunctionSetLength(TypedArrayFill, 1);


// ES6 draft 07-15-13, section 22.2.3.9
function TypedArrayFilter(f, thisArg) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);
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
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFind(predicate, thisArg, this, length);
}
%FunctionSetLength(TypedArrayFind, 1);


// ES6 draft 07-15-13, section 22.2.3.11
function TypedArrayFindIndex(predicate, thisArg) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFindIndex(predicate, thisArg, this, length);
}
%FunctionSetLength(TypedArrayFindIndex, 1);


// ES6 draft 05-18-15, section 22.2.3.21
function TypedArrayReverse() {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return PackedArrayReverse(this, length);
}


function TypedArrayComparefn(x, y) {
  if (IsNaN(x) && IsNaN(y)) {
    return IsNaN(y) ? 0 : 1;
  }
  if (IsNaN(x)) {
    return 1;
  }
  if (x === 0 && x === y) {
    if (%_IsMinusZero(x)) {
      if (!%_IsMinusZero(y)) {
        return -1;
      }
    } else if (%_IsMinusZero(y)) {
      return 1;
    }
  }
  return x - y;
}


// ES6 draft 05-18-15, section 22.2.3.25
function TypedArraySort(comparefn) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  if (IS_UNDEFINED(comparefn)) {
    comparefn = TypedArrayComparefn;
  }

  return InnerArraySort(this, length, comparefn);
}


// ES6 section 22.2.3.13
function TypedArrayIndexOf(element, index) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  return InnerArrayIndexOf(this, element, index, length);
}
%FunctionSetLength(TypedArrayIndexOf, 1);


// ES6 section 22.2.3.16
function TypedArrayLastIndexOf(element, index) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayLastIndexOf(this, element, index, length,
                        %_ArgumentsLength());
}
%FunctionSetLength(TypedArrayLastIndexOf, 1);


// ES6 draft 07-15-13, section 22.2.3.18
function TypedArrayMap(f, thisArg) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  var result = TypedArraySpeciesCreate(this, length);
  if (!IS_CALLABLE(f)) throw MakeTypeError(kCalledNonCallable, f);
  for (var i = 0; i < length; i++) {
    var element = this[i];
    result[i] = %_Call(f, thisArg, element, i, this);
  }
  return result;
}
%FunctionSetLength(TypedArrayMap, 1);


// ES6 draft 05-05-15, section 22.2.3.24
function TypedArraySome(f, receiver) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArraySome(f, receiver, this, length);
}
%FunctionSetLength(TypedArraySome, 1);


// ES6 section 22.2.3.27
function TypedArrayToLocaleString() {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayToLocaleString(this, length);
}


// ES6 section 22.2.3.28
function TypedArrayToString() {
  return %_Call(ArrayToString, this);
}


// ES6 section 22.2.3.14
function TypedArrayJoin(separator) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayJoin(separator, this, length);
}


// ES6 draft 07-15-13, section 22.2.3.19
function TypedArrayReduce(callback, current) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  return InnerArrayReduce(callback, current, this, length,
                          %_ArgumentsLength());
}
%FunctionSetLength(TypedArrayReduce, 1);


// ES6 draft 07-15-13, section 22.2.3.19
function TypedArrayReduceRight(callback, current) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  return InnerArrayReduceRight(callback, current, this, length,
                               %_ArgumentsLength());
}
%FunctionSetLength(TypedArrayReduceRight, 1);


function TypedArraySlice(start, end) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);
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
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return InnerArrayIncludes(searchElement, fromIndex, this, length);
}
%FunctionSetLength(TypedArrayIncludes, 1);


// ES6 draft 08-24-14, section 22.2.2.2
function TypedArrayOf() {
  var length = %_ArgumentsLength();
  var array = TypedArrayCreate(this, length);
  for (var i = 0; i < length; i++) {
    array[i] = %_Arguments(i);
  }
  return array;
}


function TypedArrayFrom(source, mapfn, thisArg) {
  // TODO(littledan): Investigate if there is a receiver which could be
  // faster to accumulate on than Array, e.g., a TypedVector.
  // TODO(littledan): Rewrite this code to ensure that things happen
  // in the right order, e.g., the constructor needs to be called before
  // the mapping function on array-likes.
  var array = %_Call(ArrayFrom, GlobalArray, source, mapfn, thisArg);
  return TypedArrayCreate(this, array);
}
%FunctionSetLength(TypedArrayFrom, 1);

function TypedArray() {
  if (IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kConstructorNonCallable, "TypedArray");
  }
  if (new.target === TypedArray) {
    throw MakeTypeError(kConstructAbstractClass, "TypedArray");
  }
}

// -------------------------------------------------------------------

%FunctionSetPrototype(TypedArray, new GlobalObject());
%AddNamedProperty(TypedArray.prototype,
                  "constructor", TypedArray, DONT_ENUM);
utils.InstallFunctions(TypedArray, DONT_ENUM | DONT_DELETE | READ_ONLY, [
  "from", TypedArrayFrom,
  "of", TypedArrayOf
]);
utils.InstallGetter(TypedArray.prototype, "buffer", TypedArrayGetBuffer);
utils.InstallGetter(TypedArray.prototype, "byteOffset", TypedArrayGetByteOffset,
                    DONT_ENUM | DONT_DELETE);
utils.InstallGetter(TypedArray.prototype, "byteLength",
                    TypedArrayGetByteLength, DONT_ENUM | DONT_DELETE);
utils.InstallGetter(TypedArray.prototype, "length", TypedArrayGetLength,
                    DONT_ENUM | DONT_DELETE);
utils.InstallGetter(TypedArray.prototype, toStringTagSymbol,
                    TypedArrayGetToStringTag);
utils.InstallFunctions(TypedArray.prototype, DONT_ENUM, [
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
  "toString", TypedArrayToString,
  "toLocaleString", TypedArrayToLocaleString
]);


macro SETUP_TYPED_ARRAY(ARRAY_ID, NAME, ELEMENT_SIZE)
  %SetCode(GlobalNAME, NAMEConstructor);
  %FunctionSetPrototype(GlobalNAME, new GlobalObject());
  %InternalSetPrototype(GlobalNAME, TypedArray);
  %InternalSetPrototype(GlobalNAME.prototype, TypedArray.prototype);

  %AddNamedProperty(GlobalNAME, "BYTES_PER_ELEMENT", ELEMENT_SIZE,
                    READ_ONLY | DONT_ENUM | DONT_DELETE);

  %AddNamedProperty(GlobalNAME.prototype,
                    "constructor", global.NAME, DONT_ENUM);
  %AddNamedProperty(GlobalNAME.prototype,
                    "BYTES_PER_ELEMENT", ELEMENT_SIZE,
                    READ_ONLY | DONT_ENUM | DONT_DELETE);
  // TODO(littledan): Remove this performance workaround BUG(chromium:579905)
  utils.InstallGetter(GlobalNAME.prototype, "length", NAME_GetLength,
                      DONT_ENUM | DONT_DELETE);
endmacro

TYPED_ARRAYS(SETUP_TYPED_ARRAY)

// --------------------------- DataView -----------------------------

function DataViewConstructor(buffer, byteOffset, byteLength) { // length = 3
  if (IS_UNDEFINED(new.target)) {
    throw MakeTypeError(kConstructorNotFunction, "DataView");
  }

  // TODO(binji): support SharedArrayBuffers?
  if (!IS_ARRAYBUFFER(buffer)) throw MakeTypeError(kDataViewNotArrayBuffer);
  if (!IS_UNDEFINED(byteOffset)) {
    byteOffset = ToPositiveInteger(byteOffset, kInvalidDataViewOffset);
  }
  if (!IS_UNDEFINED(byteLength)) {
    byteLength = TO_INTEGER(byteLength);
  }

  var bufferByteLength = %_ArrayBufferGetByteLength(buffer);

  var offset = IS_UNDEFINED(byteOffset) ?  0 : byteOffset;
  if (offset > bufferByteLength) throw MakeRangeError(kInvalidDataViewOffset);

  var length = IS_UNDEFINED(byteLength)
      ? bufferByteLength - offset
      : byteLength;
  if (length < 0 || offset + length > bufferByteLength) {
    throw new MakeRangeError(kInvalidDataViewLength);
  }
  var result = %NewObject(GlobalDataView, new.target);
  %_DataViewInitialize(result, buffer, offset, length);
  return result;
}

function DataViewGetBufferJS() {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver, 'DataView.buffer', this);
  }
  return %DataViewGetBuffer(this);
}

function DataViewGetByteOffset() {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'DataView.byteOffset', this);
  }
  return %_ArrayBufferViewGetByteOffset(this);
}

function DataViewGetByteLength() {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'DataView.byteLength', this);
  }
  return %_ArrayBufferViewGetByteLength(this);
}

macro DATA_VIEW_TYPES(FUNCTION)
  FUNCTION(Int8)
  FUNCTION(Uint8)
  FUNCTION(Int16)
  FUNCTION(Uint16)
  FUNCTION(Int32)
  FUNCTION(Uint32)
  FUNCTION(Float32)
  FUNCTION(Float64)
endmacro


macro DATA_VIEW_GETTER_SETTER(TYPENAME)
function DataViewGetTYPENAMEJS(offset, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'DataView.getTYPENAME', this);
  }
  if (%_ArgumentsLength() < 1) throw MakeTypeError(kInvalidArgument);
  offset = ToPositiveInteger(offset, kInvalidDataViewAccessorOffset);
  return %DataViewGetTYPENAME(this, offset, !!little_endian);
}

function DataViewSetTYPENAMEJS(offset, value, little_endian) {
  if (!IS_DATAVIEW(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'DataView.setTYPENAME', this);
  }
  if (%_ArgumentsLength() < 2) throw MakeTypeError(kInvalidArgument);
  offset = ToPositiveInteger(offset, kInvalidDataViewAccessorOffset);
  %DataViewSetTYPENAME(this, offset, TO_NUMBER(value), !!little_endian);
}
endmacro

DATA_VIEW_TYPES(DATA_VIEW_GETTER_SETTER)

// Setup the DataView constructor.
%SetCode(GlobalDataView, DataViewConstructor);
%FunctionSetPrototype(GlobalDataView, new GlobalObject);

// Set up constructor property on the DataView prototype.
%AddNamedProperty(GlobalDataView.prototype, "constructor", GlobalDataView,
                  DONT_ENUM);
%AddNamedProperty(GlobalDataView.prototype, toStringTagSymbol, "DataView",
                  READ_ONLY|DONT_ENUM);

utils.InstallGetter(GlobalDataView.prototype, "buffer", DataViewGetBufferJS);
utils.InstallGetter(GlobalDataView.prototype, "byteOffset",
                    DataViewGetByteOffset);
utils.InstallGetter(GlobalDataView.prototype, "byteLength",
                    DataViewGetByteLength);

utils.InstallFunctions(GlobalDataView.prototype, DONT_ENUM, [
  "getInt8", DataViewGetInt8JS,
  "setInt8", DataViewSetInt8JS,

  "getUint8", DataViewGetUint8JS,
  "setUint8", DataViewSetUint8JS,

  "getInt16", DataViewGetInt16JS,
  "setInt16", DataViewSetInt16JS,

  "getUint16", DataViewGetUint16JS,
  "setUint16", DataViewSetUint16JS,

  "getInt32", DataViewGetInt32JS,
  "setInt32", DataViewSetInt32JS,

  "getUint32", DataViewGetUint32JS,
  "setUint32", DataViewSetUint32JS,

  "getFloat32", DataViewGetFloat32JS,
  "setFloat32", DataViewSetFloat32JS,

  "getFloat64", DataViewGetFloat64JS,
  "setFloat64", DataViewSetFloat64JS
]);

})
