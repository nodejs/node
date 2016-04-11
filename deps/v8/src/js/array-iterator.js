// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -----------------------------------------------------------------------
// Imports

var arrayIterationKindSymbol =
    utils.ImportNow("array_iteration_kind_symbol");
var arrayIteratorNextIndexSymbol =
    utils.ImportNow("array_iterator_next_symbol");
var arrayIteratorObjectSymbol =
    utils.ImportNow("array_iterator_object_symbol");
var GlobalArray = global.Array;
var IteratorPrototype = utils.ImportNow("IteratorPrototype");
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var MakeTypeError;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");
var GlobalTypedArray = global.Uint8Array.__proto__;

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
})

// -----------------------------------------------------------------------

function ArrayIterator() {}


// TODO(wingo): Update section numbers when ES6 has stabilized.  The
// section numbers below are already out of date as of the May 2014
// draft.


// 15.4.5.1 CreateArrayIterator Abstract Operation
function CreateArrayIterator(array, kind) {
  var object = TO_OBJECT(array);
  var iterator = new ArrayIterator;
  SET_PRIVATE(iterator, arrayIteratorObjectSymbol, object);
  SET_PRIVATE(iterator, arrayIteratorNextIndexSymbol, 0);
  SET_PRIVATE(iterator, arrayIterationKindSymbol, kind);
  return iterator;
}


// 22.1.5.2.2 %ArrayIteratorPrototype%[@@iterator]
function ArrayIteratorIterator() {
    return this;
}


// ES6 section 22.1.5.2.1 %ArrayIteratorPrototype%.next( )
function ArrayIteratorNext() {
  var iterator = this;
  var value = UNDEFINED;
  var done = true;

  if (!IS_RECEIVER(iterator) ||
      !HAS_DEFINED_PRIVATE(iterator, arrayIteratorNextIndexSymbol)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'Array Iterator.prototype.next', this);
  }

  var array = GET_PRIVATE(iterator, arrayIteratorObjectSymbol);
  if (!IS_UNDEFINED(array)) {
    var index = GET_PRIVATE(iterator, arrayIteratorNextIndexSymbol);
    var itemKind = GET_PRIVATE(iterator, arrayIterationKindSymbol);
    var length = TO_UINT32(array.length);

    // "sparse" is never used.

    if (index >= length) {
      SET_PRIVATE(iterator, arrayIteratorObjectSymbol, UNDEFINED);
    } else {
      SET_PRIVATE(iterator, arrayIteratorNextIndexSymbol, index + 1);

      if (itemKind == ITERATOR_KIND_VALUES) {
        value = array[index];
      } else if (itemKind == ITERATOR_KIND_ENTRIES) {
        value = [index, array[index]];
      } else {
        value = index;
      }
      done = false;
    }
  }

  return %_CreateIterResultObject(value, done);
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


%FunctionSetPrototype(ArrayIterator, {__proto__: IteratorPrototype});
%FunctionSetInstanceClassName(ArrayIterator, 'Array Iterator');

utils.InstallFunctions(ArrayIterator.prototype, DONT_ENUM, [
  'next', ArrayIteratorNext
]);
utils.SetFunctionName(ArrayIteratorIterator, iteratorSymbol);
%AddNamedProperty(ArrayIterator.prototype, iteratorSymbol,
                  ArrayIteratorIterator, DONT_ENUM);
%AddNamedProperty(ArrayIterator.prototype, toStringTagSymbol,
                  "Array Iterator", READ_ONLY | DONT_ENUM);

utils.InstallFunctions(GlobalArray.prototype, DONT_ENUM, [
  // No 'values' since it breaks webcompat: http://crbug.com/409858
  'entries', ArrayEntries,
  'keys', ArrayKeys
]);

// TODO(adam): Remove this call once 'values' is in the above
// InstallFunctions block, as it'll be redundant.
utils.SetFunctionName(ArrayValues, 'values');

%AddNamedProperty(GlobalArray.prototype, iteratorSymbol, ArrayValues,
                  DONT_ENUM);

%AddNamedProperty(GlobalTypedArray.prototype,
                  'entries', ArrayEntries, DONT_ENUM);
%AddNamedProperty(GlobalTypedArray.prototype, 'values', ArrayValues, DONT_ENUM);
%AddNamedProperty(GlobalTypedArray.prototype, 'keys', ArrayKeys, DONT_ENUM);
%AddNamedProperty(GlobalTypedArray.prototype,
                  iteratorSymbol, ArrayValues, DONT_ENUM);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.ArrayValues = ArrayValues;
});

%InstallToContext(["array_values_iterator", ArrayValues]);

})
