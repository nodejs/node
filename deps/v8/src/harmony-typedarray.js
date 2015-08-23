// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

macro TYPED_ARRAYS(FUNCTION)
// arrayIds below should be synchronized with Runtime_TypedArrayInitialize.
FUNCTION(Uint8Array)
FUNCTION(Int8Array)
FUNCTION(Uint16Array)
FUNCTION(Int16Array)
FUNCTION(Uint32Array)
FUNCTION(Int32Array)
FUNCTION(Float32Array)
FUNCTION(Float64Array)
FUNCTION(Uint8ClampedArray)
endmacro

macro DECLARE_GLOBALS(NAME)
var GlobalNAME = global.NAME;
endmacro

TYPED_ARRAYS(DECLARE_GLOBALS)
DECLARE_GLOBALS(Array)

var ArrayFrom;
var ArrayToString;
var InnerArrayCopyWithin;
var InnerArrayEvery;
var InnerArrayFill;
var InnerArrayFilter;
var InnerArrayFind;
var InnerArrayFindIndex;
var InnerArrayForEach;
var InnerArrayIndexOf;
var InnerArrayJoin;
var InnerArrayLastIndexOf;
var InnerArrayMap;
var InnerArrayReverse;
var InnerArraySome;
var InnerArraySort;
var InnerArrayToLocaleString;
var IsNaN;
var MathMax;
var MathMin;

utils.Import(function(from) {
  ArrayFrom = from.ArrayFrom;
  ArrayToString = from.ArrayToString;
  InnerArrayCopyWithin = from.InnerArrayCopyWithin;
  InnerArrayEvery = from.InnerArrayEvery;
  InnerArrayFill = from.InnerArrayFill;
  InnerArrayFilter = from.InnerArrayFilter;
  InnerArrayFind = from.InnerArrayFind;
  InnerArrayFindIndex = from.InnerArrayFindIndex;
  InnerArrayForEach = from.InnerArrayForEach;
  InnerArrayIndexOf = from.InnerArrayIndexOf;
  InnerArrayJoin = from.InnerArrayJoin;
  InnerArrayLastIndexOf = from.InnerArrayLastIndexOf;
  InnerArrayMap = from.InnerArrayMap;
  InnerArrayReduce = from.InnerArrayReduce;
  InnerArrayReduceRight = from.InnerArrayReduceRight;
  InnerArrayReverse = from.InnerArrayReverse;
  InnerArraySome = from.InnerArraySome;
  InnerArraySort = from.InnerArraySort;
  InnerArrayToLocaleString = from.InnerArrayToLocaleString;
  IsNaN = from.IsNaN;
  MathMax = from.MathMax;
  MathMin = from.MathMin;
});

// -------------------------------------------------------------------

function ConstructTypedArray(constructor, arg) {
  // TODO(littledan): This is an approximation of the spec, which requires
  // that only real TypedArray classes should be accepted (22.2.2.1.1)
  if (!%IsConstructor(constructor) || IS_UNDEFINED(constructor.prototype) ||
      !%HasOwnProperty(constructor.prototype, "BYTES_PER_ELEMENT")) {
    throw MakeTypeError(kNotTypedArray);
  }

  // TODO(littledan): The spec requires that, rather than directly calling
  // the constructor, a TypedArray is created with the proper proto and
  // underlying size and element size, and elements are put in one by one.
  // By contrast, this would allow subclasses to make a radically different
  // constructor with different semantics.
  return new constructor(arg);
}

function ConstructTypedArrayLike(typedArray, arg) {
  // TODO(littledan): The spec requires that we actuallly use
  // typedArray.constructor[Symbol.species] (bug v8:4093)
  // Also, it should default to the default constructor from
  // table 49 if typedArray.constructor doesn't exist.
  return ConstructTypedArray(typedArray.constructor, arg);
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
function TypedArrayFilter(predicate, thisArg) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);
  var array = InnerArrayFilter(predicate, thisArg, this, length);
  return ConstructTypedArrayLike(this, array);
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

  return InnerArrayReverse(this, length);
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

  return %_CallFunction(this, length, comparefn, InnerArraySort);
}


// ES6 section 22.2.3.13
function TypedArrayIndexOf(element, index) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return %_CallFunction(this, element, index, length, InnerArrayIndexOf);
}
%FunctionSetLength(TypedArrayIndexOf, 1);


// ES6 section 22.2.3.16
function TypedArrayLastIndexOf(element, index) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  var length = %_TypedArrayGetLength(this);

  return %_CallFunction(this, element, index, length,
                        %_ArgumentsLength(), InnerArrayLastIndexOf);
}
%FunctionSetLength(TypedArrayLastIndexOf, 1);


// ES6 draft 07-15-13, section 22.2.3.18
function TypedArrayMap(predicate, thisArg) {
  if (!%_IsTypedArray(this)) throw MakeTypeError(kNotTypedArray);

  // TODO(littledan): Preallocate rather than making an intermediate
  // InternalArray, for better performance.
  var length = %_TypedArrayGetLength(this);
  var array = InnerArrayMap(predicate, thisArg, this, length);
  return ConstructTypedArrayLike(this, array);
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
  return %_CallFunction(this, ArrayToString);
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
    k = MathMax(len + relativeStart, 0);
  } else {
    k = MathMin(relativeStart, len);
  }

  var relativeEnd;
  if (IS_UNDEFINED(end)) {
    relativeEnd = len;
  } else {
    relativeEnd = TO_INTEGER(end);
  }

  var final;
  if (relativeEnd < 0) {
    final = MathMax(len + relativeEnd, 0);
  } else {
    final = MathMin(relativeEnd, len);
  }

  var count = MathMax(final - k, 0);
  var array = ConstructTypedArrayLike(this, count);
  // The code below is the 'then' branch; the 'else' branch species
  // a memcpy. Because V8 doesn't canonicalize NaN, the difference is
  // unobservable.
  var n = 0;
  while (k < final) {
    var kValue = this[k];
    // TODO(littledan): The spec says to throw on an error in setting;
    // does this throw?
    array[n] = kValue;
    k++;
    n++;
  }
  return array;
}


// ES6 draft 08-24-14, section 22.2.2.2
function TypedArrayOf() {
  var length = %_ArgumentsLength();
  var array = new this(length);
  for (var i = 0; i < length; i++) {
    array[i] = %_Arguments(i);
  }
  return array;
}


function TypedArrayFrom(source, mapfn, thisArg) {
  // TODO(littledan): Investigate if there is a receiver which could be
  // faster to accumulate on than Array, e.g., a TypedVector.
  var array = %_CallFunction(GlobalArray, source, mapfn, thisArg, ArrayFrom);
  return ConstructTypedArray(this, array);
}
%FunctionSetLength(TypedArrayFrom, 1);

// TODO(littledan): Fix the TypedArray proto chain (bug v8:4085).
macro EXTEND_TYPED_ARRAY(NAME)
  // Set up non-enumerable functions on the object.
  utils.InstallFunctions(GlobalNAME, DONT_ENUM | DONT_DELETE | READ_ONLY, [
    "from", TypedArrayFrom,
    "of", TypedArrayOf
  ]);

  // Set up non-enumerable functions on the prototype object.
  utils.InstallFunctions(GlobalNAME.prototype, DONT_ENUM, [
    "copyWithin", TypedArrayCopyWithin,
    "every", TypedArrayEvery,
    "fill", TypedArrayFill,
    "filter", TypedArrayFilter,
    "find", TypedArrayFind,
    "findIndex", TypedArrayFindIndex,
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
endmacro

TYPED_ARRAYS(EXTEND_TYPED_ARRAY)

})
