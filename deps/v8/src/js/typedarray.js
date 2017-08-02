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
var GetIterator;
var GetMethod;
var GlobalArray = global.Array;
var GlobalArrayBuffer = global.ArrayBuffer;
var GlobalArrayBufferPrototype = GlobalArrayBuffer.prototype;
var GlobalObject = global.Object;
var InnerArrayFind;
var InnerArrayFindIndex;
var InnerArrayJoin;
var InnerArraySort;
var InnerArrayToLocaleString;
var InternalArray = utils.InternalArray;
var MaxSimple;
var MinSimple;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var speciesSymbol = utils.ImportNow("species_symbol");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

macro TYPED_ARRAYS(FUNCTION)
FUNCTION(Uint8Array, 1)
FUNCTION(Int8Array, 1)
FUNCTION(Uint16Array, 2)
FUNCTION(Int16Array, 2)
FUNCTION(Uint32Array, 4)
FUNCTION(Int32Array, 4)
FUNCTION(Float32Array, 4)
FUNCTION(Float64Array, 8)
FUNCTION(Uint8ClampedArray, 1)
endmacro

macro DECLARE_GLOBALS(NAME, SIZE)
var GlobalNAME = global.NAME;
endmacro

TYPED_ARRAYS(DECLARE_GLOBALS)

var GlobalTypedArray = %object_get_prototype_of(GlobalUint8Array);

utils.Import(function(from) {
  GetIterator = from.GetIterator;
  GetMethod = from.GetMethod;
  InnerArrayFind = from.InnerArrayFind;
  InnerArrayFindIndex = from.InnerArrayFindIndex;
  InnerArrayJoin = from.InnerArrayJoin;
  InnerArraySort = from.InnerArraySort;
  InnerArrayToLocaleString = from.InnerArrayToLocaleString;
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
});

// ES2015 7.3.20
function SpeciesConstructor(object, defaultConstructor) {
  var constructor = object.constructor;
  if (IS_UNDEFINED(constructor)) {
    return defaultConstructor;
  }
  if (!IS_RECEIVER(constructor)) {
    throw %make_type_error(kConstructorNotReceiver);
  }
  var species = constructor[speciesSymbol];
  if (IS_NULL_OR_UNDEFINED(species)) {
    return defaultConstructor;
  }
  if (%IsConstructor(species)) {
    return species;
  }
  throw %make_type_error(kSpeciesNotConstructor);
}

// --------------- Typed Arrays ---------------------

// ES6 section 22.2.3.5.1 ValidateTypedArray ( O )
function ValidateTypedArray(array, methodName) {
  if (!IS_TYPEDARRAY(array)) throw %make_type_error(kNotTypedArray);

  if (%_ArrayBufferViewWasNeutered(array))
    throw %make_type_error(kDetachedOperation, methodName);
}

function TypedArrayDefaultConstructor(typedArray) {
  switch (%_ClassOf(typedArray)) {
macro TYPED_ARRAY_CONSTRUCTOR_CASE(NAME, ELEMENT_SIZE)
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
  ValidateTypedArray(newTypedArray, "TypedArrayCreate");
  if (IS_NUMBER(arg0) && %_TypedArrayGetLength(newTypedArray) < arg0) {
    throw %make_type_error(kTypedArrayTooShort);
  }
  return newTypedArray;
}

function TypedArraySpeciesCreate(exemplar, arg0, arg1, arg2) {
  var defaultConstructor = TypedArrayDefaultConstructor(exemplar);
  var constructor = SpeciesConstructor(exemplar, defaultConstructor);
  return TypedArrayCreate(constructor, arg0, arg1, arg2);
}

macro TYPED_ARRAY_CONSTRUCTOR(NAME, ELEMENT_SIZE)
function NAMEConstructByIterable(obj, iterable, iteratorFn) {
  if (%IterableToListCanBeElided(iterable)) {
    // This .length access is unobservable, because it being observable would
    // mean that iteration has side effects, and we wouldn't reach this path.
    %typed_array_construct_by_array_like(
        obj, iterable, iterable.length, ELEMENT_SIZE);
  } else {
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
    %typed_array_construct_by_array_like(obj, list, list.length, ELEMENT_SIZE);
  }
}

// ES#sec-typedarray-typedarray TypedArray ( typedArray )
function NAMEConstructByTypedArray(obj, typedArray) {
  // TODO(littledan): Throw on detached typedArray
  var srcData = %TypedArrayGetBuffer(typedArray);
  var length = %_TypedArrayGetLength(typedArray);
  var byteLength = %_ArrayBufferViewGetByteLength(typedArray);
  var newByteLength = length * ELEMENT_SIZE;
  %typed_array_construct_by_array_like(obj, typedArray, length, ELEMENT_SIZE);
  // The spec requires that constructing a typed array using a SAB-backed typed
  // array use the ArrayBuffer constructor, not the species constructor. See
  // https://tc39.github.io/ecma262/#sec-typedarray-typedarray.
  var bufferConstructor = IS_SHAREDARRAYBUFFER(srcData)
                            ? GlobalArrayBuffer
                            : SpeciesConstructor(srcData, GlobalArrayBuffer);
  var prototype = bufferConstructor.prototype;
  // TODO(littledan): Use the right prototype based on bufferConstructor's realm
  if (IS_RECEIVER(prototype) && prototype !== GlobalArrayBufferPrototype) {
    %InternalSetPrototype(%TypedArrayGetBuffer(obj), prototype);
  }
}

function NAMEConstructor(arg1, arg2, arg3) {
  if (!IS_UNDEFINED(new.target)) {
    if (IS_ARRAYBUFFER(arg1) || IS_SHAREDARRAYBUFFER(arg1)) {
      %typed_array_construct_by_array_buffer(
          this, arg1, arg2, arg3, ELEMENT_SIZE);
    } else if (IS_TYPEDARRAY(arg1)) {
      NAMEConstructByTypedArray(this, arg1);
    } else if (IS_RECEIVER(arg1)) {
      var iteratorFn = arg1[iteratorSymbol];
      if (IS_UNDEFINED(iteratorFn)) {
        %typed_array_construct_by_array_like(
            this, arg1, arg1.length, ELEMENT_SIZE);
      } else {
        NAMEConstructByIterable(this, arg1, iteratorFn);
      }
    } else {
      %typed_array_construct_by_length(this, arg1, ELEMENT_SIZE);
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
  return TypedArraySpeciesCreate(this, %TypedArrayGetBuffer(this),
                                 beginByteOffset, newLength);
}
endmacro

TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTOR)

function TypedArraySubArray(begin, end) {
  switch (%_ClassOf(this)) {
macro TYPED_ARRAY_SUBARRAY_CASE(NAME, ELEMENT_SIZE)
    case "NAME":
      return %_Call(NAMESubArray, this, begin, end);
endmacro
TYPED_ARRAYS(TYPED_ARRAY_SUBARRAY_CASE)
  }
  throw %make_type_error(kIncompatibleMethodReceiver,
                      "get %TypedArray%.prototype.subarray", this);
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
    var targetPtr = %_ArrayBufferViewGetByteOffset(target) +
                    (offset + 1) * targetElementSize;
    // Next read at sourcePtr. We do not care for memory changing before
    // sourcePtr - we have already copied it.
    var sourcePtr = %_ArrayBufferViewGetByteOffset(source);
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
    var targetPtr = %_ArrayBufferViewGetByteOffset(target) +
                    (offset + sourceLength - 1) * targetElementSize;
    // Next read before sourcePtr. We do not care for memory changing after
    // sourcePtr - we have already copied it.
    var sourcePtr = %_ArrayBufferViewGetByteOffset(source) +
                    sourceLength * sourceElementSize;
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
      if (intOffset === 0) {
        %TypedArrayCopyElements(this, obj, %_TypedArrayGetLength(obj));
      } else {
        TypedArraySetFromArrayLike(
            this, obj, %_TypedArrayGetLength(obj), intOffset);
      }
      return;
    case 3: // TYPED_ARRAY_SET_NON_TYPED_ARRAY
      var l = obj.length;
      if (IS_UNDEFINED(l)) {
        if (IS_NUMBER(obj)) {
            // For number as a first argument, throw TypeError
            // instead of silently ignoring the call, so that
            // users know they did something wrong.
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

function InnerTypedArrayForEach(f, receiver, array, length) {
  if (!IS_CALLABLE(f)) throw %make_type_error(kCalledNonCallable, f);

  if (IS_UNDEFINED(receiver)) {
    for (var i = 0; i < length; i++) {
      if (i in array) {
        var element = array[i];
        f(element, i, array);
      }
    }
  } else {
    for (var i = 0; i < length; i++) {
      if (i in array) {
        var element = array[i];
        %_Call(f, receiver, element, i, array);
      }
    }
  }
}

// ES6 draft 08-24-14, section 22.2.3.12
function TypedArrayForEach(f, receiver) {
  ValidateTypedArray(this, "%TypedArray%.prototype.forEach");

  var length = %_TypedArrayGetLength(this);

  InnerTypedArrayForEach(f, receiver, this, length);
}
%FunctionSetLength(TypedArrayForEach, 1);

// The following functions cannot be made efficient on sparse arrays while
// preserving the semantics, since the calls to the receiver function can add
// or delete elements from the array.
function InnerTypedArrayFilter(f, receiver, array, length, result) {
  var result_length = 0;
  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      if (%_Call(f, receiver, element, i, array)) {
        %CreateDataProperty(result, result_length, element);
        result_length++;
      }
    }
  }
  return result;
}


// ES6 draft 07-15-13, section 22.2.3.9
function TypedArrayFilter(f, thisArg) {
  ValidateTypedArray(this, "%TypeArray%.prototype.filter");

  var length = %_TypedArrayGetLength(this);
  if (!IS_CALLABLE(f)) throw %make_type_error(kCalledNonCallable, f);
  var result = new InternalArray();
  InnerTypedArrayFilter(f, thisArg, this, length, result);
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
  ValidateTypedArray(this, "%TypedArray%.prototype.find");

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFind(predicate, thisArg, this, length);
}
%FunctionSetLength(TypedArrayFind, 1);


// ES6 draft 07-15-13, section 22.2.3.11
function TypedArrayFindIndex(predicate, thisArg) {
  ValidateTypedArray(this, "%TypedArray%.prototype.findIndex");

  var length = %_TypedArrayGetLength(this);

  return InnerArrayFindIndex(predicate, thisArg, this, length);
}
%FunctionSetLength(TypedArrayFindIndex, 1);


// ES6 draft 05-18-15, section 22.2.3.25
function TypedArraySort(comparefn) {
  ValidateTypedArray(this, "%TypedArray%.prototype.sort");

  var length = %_TypedArrayGetLength(this);

  if (IS_UNDEFINED(comparefn)) {
    return %TypedArraySortFast(this);
  }

  return InnerArraySort(this, length, comparefn);
}


// ES6 section 22.2.3.27
function TypedArrayToLocaleString() {
  ValidateTypedArray(this, "%TypedArray%.prototype.toLocaleString");

  var length = %_TypedArrayGetLength(this);

  return InnerArrayToLocaleString(this, length);
}


// ES6 section 22.2.3.14
function TypedArrayJoin(separator) {
  ValidateTypedArray(this, "%TypedArray%.prototype.join");

  var length = %_TypedArrayGetLength(this);

  return InnerArrayJoin(separator, this, length);
}


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
  throw %make_type_error(kConstructAbstractClass, "TypedArray");
}

// -------------------------------------------------------------------

%SetCode(GlobalTypedArray, TypedArrayConstructor);
utils.InstallFunctions(GlobalTypedArray, DONT_ENUM, [
  "from", TypedArrayFrom,
  "of", TypedArrayOf
]);
utils.InstallGetter(GlobalTypedArray.prototype, toStringTagSymbol,
                    TypedArrayGetToStringTag);
utils.InstallFunctions(GlobalTypedArray.prototype, DONT_ENUM, [
  "subarray", TypedArraySubArray,
  "set", TypedArraySet,
  "filter", TypedArrayFilter,
  "find", TypedArrayFind,
  "findIndex", TypedArrayFindIndex,
  "join", TypedArrayJoin,
  "forEach", TypedArrayForEach,
  "sort", TypedArraySort,
  "toLocaleString", TypedArrayToLocaleString
]);

%AddNamedProperty(GlobalTypedArray.prototype, "toString", ArrayToString,
                  DONT_ENUM);


macro SETUP_TYPED_ARRAY(NAME, ELEMENT_SIZE)
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
