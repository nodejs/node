// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $arrayValues;

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var GlobalArray = global.Array;

macro TYPED_ARRAYS(FUNCTION)
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

macro COPY_FROM_GLOBAL(NAME)
  var GlobalNAME = global.NAME;
endmacro

TYPED_ARRAYS(COPY_FROM_GLOBAL)

var arrayIteratorObjectSymbol = GLOBAL_PRIVATE("ArrayIterator#object");
var arrayIteratorNextIndexSymbol = GLOBAL_PRIVATE("ArrayIterator#next");
var arrayIterationKindSymbol = GLOBAL_PRIVATE("ArrayIterator#kind");


function ArrayIterator() {}


// TODO(wingo): Update section numbers when ES6 has stabilized.  The
// section numbers below are already out of date as of the May 2014
// draft.


// 15.4.5.1 CreateArrayIterator Abstract Operation
function CreateArrayIterator(array, kind) {
  var object = $toObject(array);
  var iterator = new ArrayIterator;
  SET_PRIVATE(iterator, arrayIteratorObjectSymbol, object);
  SET_PRIVATE(iterator, arrayIteratorNextIndexSymbol, 0);
  SET_PRIVATE(iterator, arrayIterationKindSymbol, kind);
  return iterator;
}


// 15.19.4.3.4 CreateItrResultObject
function CreateIteratorResultObject(value, done) {
  return {value: value, done: done};
}


// 22.1.5.2.2 %ArrayIteratorPrototype%[@@iterator]
function ArrayIteratorIterator() {
    return this;
}


// 15.4.5.2.2 ArrayIterator.prototype.next( )
function ArrayIteratorNext() {
  var iterator = $toObject(this);

  if (!HAS_DEFINED_PRIVATE(iterator, arrayIteratorNextIndexSymbol)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Array Iterator.prototype.next', this);
  }

  var array = GET_PRIVATE(iterator, arrayIteratorObjectSymbol);
  if (IS_UNDEFINED(array)) {
    return CreateIteratorResultObject(UNDEFINED, true);
  }

  var index = GET_PRIVATE(iterator, arrayIteratorNextIndexSymbol);
  var itemKind = GET_PRIVATE(iterator, arrayIterationKindSymbol);
  var length = TO_UINT32(array.length);

  // "sparse" is never used.

  if (index >= length) {
    SET_PRIVATE(iterator, arrayIteratorObjectSymbol, UNDEFINED);
    return CreateIteratorResultObject(UNDEFINED, true);
  }

  SET_PRIVATE(iterator, arrayIteratorNextIndexSymbol, index + 1);

  if (itemKind == ITERATOR_KIND_VALUES) {
    return CreateIteratorResultObject(array[index], false);
  }

  if (itemKind == ITERATOR_KIND_ENTRIES) {
    return CreateIteratorResultObject([index, array[index]], false);
  }

  return CreateIteratorResultObject(index, false);
}


function ArrayEntries() {
  return CreateArrayIterator(this, ITERATOR_KIND_ENTRIES);
}


function ArrayValues() {
  return CreateArrayIterator(this, ITERATOR_KIND_VALUES);
}


function ArrayKeys() {
  return CreateArrayIterator(this, ITERATOR_KIND_KEYS);
}


%FunctionSetPrototype(ArrayIterator, {__proto__: $iteratorPrototype});
%FunctionSetInstanceClassName(ArrayIterator, 'Array Iterator');

utils.InstallFunctions(ArrayIterator.prototype, DONT_ENUM, [
  'next', ArrayIteratorNext
]);
utils.SetFunctionName(ArrayIteratorIterator, symbolIterator);
%AddNamedProperty(ArrayIterator.prototype, symbolIterator,
                  ArrayIteratorIterator, DONT_ENUM);
%AddNamedProperty(ArrayIterator.prototype, symbolToStringTag,
                  "Array Iterator", READ_ONLY | DONT_ENUM);

utils.InstallFunctions(GlobalArray.prototype, DONT_ENUM, [
  // No 'values' since it breaks webcompat: http://crbug.com/409858
  'entries', ArrayEntries,
  'keys', ArrayKeys
]);

%AddNamedProperty(GlobalArray.prototype, symbolIterator, ArrayValues,
                  DONT_ENUM);

macro EXTEND_TYPED_ARRAY(NAME)
  %AddNamedProperty(GlobalNAME.prototype, 'entries', ArrayEntries, DONT_ENUM);
  %AddNamedProperty(GlobalNAME.prototype, 'values', ArrayValues, DONT_ENUM);
  %AddNamedProperty(GlobalNAME.prototype, 'keys', ArrayKeys, DONT_ENUM);
  %AddNamedProperty(GlobalNAME.prototype, symbolIterator, ArrayValues,
                    DONT_ENUM);
endmacro

TYPED_ARRAYS(EXTEND_TYPED_ARRAY)

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.ArrayIteratorCreateResultObject = CreateIteratorResultObject;
});

$arrayValues = ArrayValues;

})
