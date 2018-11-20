// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

// -------------------------------------------------------------------

// ES6 draft 07-15-13, section 15.4.3.23
function ArrayFind(predicate /* thisArg */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.find");

  var array = ToObject(this);
  var length = ToInteger(array.length);

  if (!IS_SPEC_FUNCTION(predicate)) {
    throw MakeTypeError('called_non_callable', [predicate]);
  }

  var thisArg;
  if (%_ArgumentsLength() > 1) {
    thisArg = %_Arguments(1);
  }

  var needs_wrapper = false;
  if (IS_NULL_OR_UNDEFINED(thisArg)) {
    thisArg = %GetDefaultReceiver(predicate) || thisArg;
  } else {
    needs_wrapper = SHOULD_CREATE_WRAPPER(predicate, thisArg);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      var newThisArg = needs_wrapper ? ToObject(thisArg) : thisArg;
      if (%_CallFunction(newThisArg, element, i, array, predicate)) {
        return element;
      }
    }
  }

  return;
}


// ES6 draft 07-15-13, section 15.4.3.24
function ArrayFindIndex(predicate /* thisArg */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.findIndex");

  var array = ToObject(this);
  var length = ToInteger(array.length);

  if (!IS_SPEC_FUNCTION(predicate)) {
    throw MakeTypeError('called_non_callable', [predicate]);
  }

  var thisArg;
  if (%_ArgumentsLength() > 1) {
    thisArg = %_Arguments(1);
  }

  var needs_wrapper = false;
  if (IS_NULL_OR_UNDEFINED(thisArg)) {
    thisArg = %GetDefaultReceiver(predicate) || thisArg;
  } else {
    needs_wrapper = SHOULD_CREATE_WRAPPER(predicate, thisArg);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      var newThisArg = needs_wrapper ? ToObject(thisArg) : thisArg;
      if (%_CallFunction(newThisArg, element, i, array, predicate)) {
        return i;
      }
    }
  }

  return -1;
}


// ES6, draft 04-05-14, section 22.1.3.6
function ArrayFill(value /* [, start [, end ] ] */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.fill");

  var array = ToObject(this);
  var length = TO_UINT32(array.length);

  var i = 0;
  var end = length;

  if (%_ArgumentsLength() > 1) {
    i = %_Arguments(1);
    i = IS_UNDEFINED(i) ? 0 : TO_INTEGER(i);
    if (%_ArgumentsLength() > 2) {
      end = %_Arguments(2);
      end = IS_UNDEFINED(end) ? length : TO_INTEGER(end);
    }
  }

  if (i < 0) {
    i += length;
    if (i < 0) i = 0;
  } else {
    if (i > length) i = length;
  }

  if (end < 0) {
    end += length;
    if (end < 0) end = 0;
  } else {
    if (end > length) end = length;
  }

  if ((end - i) > 0 && ObjectIsFrozen(array)) {
    throw MakeTypeError("array_functions_on_frozen",
                        ["Array.prototype.fill"]);
  }

  for (; i < end; i++)
    array[i] = value;
  return array;
}

// ES6, draft 10-14-14, section 22.1.2.1
function ArrayFrom(arrayLike, mapfn, receiver) {
  var items = ToObject(arrayLike);
  var mapping = !IS_UNDEFINED(mapfn);

  if (mapping) {
    if (!IS_SPEC_FUNCTION(mapfn)) {
      throw MakeTypeError('called_non_callable', [ mapfn ]);
    } else if (IS_NULL_OR_UNDEFINED(receiver)) {
      receiver = %GetDefaultReceiver(mapfn) || receiver;
    } else if (!IS_SPEC_OBJECT(receiver) && %IsSloppyModeFunction(mapfn)) {
      receiver = ToObject(receiver);
    }
  }

  var iterable = GetMethod(items, symbolIterator);
  var k;
  var result;
  var mappedValue;
  var nextValue;

  if (!IS_UNDEFINED(iterable)) {
    result = %IsConstructor(this) ? new this() : [];

    var iterator = GetIterator(items, iterable);

    k = 0;
    while (true) {
      var next = iterator.next();

      if (!IS_OBJECT(next)) {
        throw MakeTypeError("iterator_result_not_an_object", [next]);
      }

      if (next.done) {
        result.length = k;
        return result;
      }

      nextValue = next.value;
      if (mapping) {
        mappedValue = %_CallFunction(receiver, nextValue, k, mapfn);
      } else {
        mappedValue = nextValue;
      }
      %AddElement(result, k++, mappedValue, NONE);
    }
  } else {
    var len = ToLength(items.length);
    result = %IsConstructor(this) ? new this(len) : new $Array(len);

    for (k = 0; k < len; ++k) {
      nextValue = items[k];
      if (mapping) {
        mappedValue = %_CallFunction(receiver, nextValue, k, mapfn);
      } else {
        mappedValue = nextValue;
      }
      %AddElement(result, k, mappedValue, NONE);
    }

    result.length = k;
    return result;
  }
}

// ES6, draft 05-22-14, section 22.1.2.3
function ArrayOf() {
  var length = %_ArgumentsLength();
  var constructor = this;
  // TODO: Implement IsConstructor (ES6 section 7.2.5)
  var array = %IsConstructor(constructor) ? new constructor(length) : [];
  for (var i = 0; i < length; i++) {
    %AddElement(array, i, %_Arguments(i), NONE);
  }
  array.length = length;
  return array;
}

// -------------------------------------------------------------------

function HarmonyArrayExtendSymbolPrototype() {
  %CheckIsBootstrapping();

  InstallConstants($Symbol, $Array(
    // TODO(dslomov, caitp): Move to symbol.js when shipping
   "isConcatSpreadable", symbolIsConcatSpreadable
  ));
}

HarmonyArrayExtendSymbolPrototype();

function HarmonyArrayExtendArrayPrototype() {
  %CheckIsBootstrapping();

  %FunctionSetLength(ArrayFrom, 1);

  // Set up non-enumerable functions on the Array object.
  InstallFunctions($Array, DONT_ENUM, $Array(
    "from", ArrayFrom,
    "of", ArrayOf
  ));

  // Set up the non-enumerable functions on the Array prototype object.
  InstallFunctions($Array.prototype, DONT_ENUM, $Array(
    "find", ArrayFind,
    "findIndex", ArrayFindIndex,
    "fill", ArrayFill
  ));
}

HarmonyArrayExtendArrayPrototype();
