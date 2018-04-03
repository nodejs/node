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
var InnerArrayJoin;
var InnerArraySort;
var InnerArrayToLocaleString;
var InternalArray = utils.InternalArray;
var MathMax = global.Math.max;
var MathMin = global.Math.min;
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

macro IS_ARRAYBUFFER(arg)
(%_ClassOf(arg) === 'ArrayBuffer')
endmacro

macro IS_SHAREDARRAYBUFFER(arg)
(%_ClassOf(arg) === 'SharedArrayBuffer')
endmacro

macro IS_TYPEDARRAY(arg)
(%_IsTypedArray(arg))
endmacro

var GlobalTypedArray = %object_get_prototype_of(GlobalUint8Array);

utils.Import(function(from) {
  GetIterator = from.GetIterator;
  GetMethod = from.GetMethod;
  InnerArrayJoin = from.InnerArrayJoin;
  InnerArraySort = from.InnerArraySort;
  InnerArrayToLocaleString = from.InnerArrayToLocaleString;
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
    beginInt = MathMax(0, srcLength + beginInt);
  } else {
    beginInt = MathMin(beginInt, srcLength);
  }

  if (endInt < 0) {
    endInt = MathMax(0, srcLength + endInt);
  } else {
    endInt = MathMin(endInt, srcLength);
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

DEFINE_METHOD(
  GlobalTypedArray.prototype,
  subarray(begin, end) {
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
);


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
DEFINE_METHOD_LEN(
  GlobalTypedArray.prototype,
  filter(f, thisArg) {
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
  },
  1  /* Set function length. */
);

// ES6 draft 05-18-15, section 22.2.3.25
DEFINE_METHOD(
  GlobalTypedArray.prototype,
  sort(comparefn) {
    ValidateTypedArray(this, "%TypedArray%.prototype.sort");

    if (!IS_UNDEFINED(comparefn) && !IS_CALLABLE(comparefn)) {
      throw %make_type_error(kBadSortComparisonFunction, comparefn);
    }

    var length = %_TypedArrayGetLength(this);

    if (IS_UNDEFINED(comparefn)) {
      return %TypedArraySortFast(this);
    }

    return InnerArraySort(this, length, comparefn);
  }
);


// ES6 section 22.2.3.27
DEFINE_METHOD(
  GlobalTypedArray.prototype,
  toLocaleString() {
    ValidateTypedArray(this, "%TypedArray%.prototype.toLocaleString");

    var length = %_TypedArrayGetLength(this);

    return InnerArrayToLocaleString(this, length);
  }
);


// ES6 section 22.2.3.14
DEFINE_METHOD(
  GlobalTypedArray.prototype,
  join(separator) {
    ValidateTypedArray(this, "%TypedArray%.prototype.join");

    var length = %_TypedArrayGetLength(this);

    return InnerArrayJoin(separator, this, length);
  }
);


// ES6 draft 08-24-14, section 22.2.2.2
DEFINE_METHOD(
  GlobalTypedArray,
  of() {
    var length = arguments.length;
    var array = TypedArrayCreate(this, length);
    for (var i = 0; i < length; i++) {
      array[i] = arguments[i];
    }
    return array;
  }
);


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
DEFINE_METHOD_LEN(
  GlobalTypedArray,
  'from'(source, mapfn, thisArg) {
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
  },
  1  /* Set function length. */
);

// TODO(bmeurer): Migrate this to a proper builtin.
function TypedArrayConstructor() {
  throw %make_type_error(kConstructAbstractClass, "TypedArray");
}

// -------------------------------------------------------------------

%SetCode(GlobalTypedArray, TypedArrayConstructor);


%AddNamedProperty(GlobalTypedArray.prototype, "toString", ArrayToString,
                  DONT_ENUM);


macro SETUP_TYPED_ARRAY(NAME, ELEMENT_SIZE)
  %SetCode(GlobalNAME, NAMEConstructor);
endmacro

TYPED_ARRAYS(SETUP_TYPED_ARRAY)

})
