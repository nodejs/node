// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, shared, exports) {

'use strict';

%CheckIsBootstrapping();

var GlobalArray = global.Array;
var GlobalSymbol = global.Symbol;

// -------------------------------------------------------------------

// ES6 draft 03-17-15, section 22.1.3.3
function ArrayCopyWithin(target, start, end) {
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.copyWithin");

  var array = TO_OBJECT_INLINE(this);
  var length = $toLength(array.length);

  target = TO_INTEGER(target);
  var to;
  if (target < 0) {
    to = $max(length + target, 0);
  } else {
    to = $min(target, length);
  }

  start = TO_INTEGER(start);
  var from;
  if (start < 0) {
    from = $max(length + start, 0);
  } else {
    from = $min(start, length);
  }

  end = IS_UNDEFINED(end) ? length : TO_INTEGER(end);
  var final;
  if (end < 0) {
    final = $max(length + end, 0);
  } else {
    final = $min(end, length);
  }

  var count = $min(final - from, length - to);
  var direction = 1;
  if (from < to && to < (from + count)) {
    direction = -1;
    from = from + count - 1;
    to = to + count - 1;
  }

  while (count > 0) {
    if (from in array) {
      array[to] = array[from];
    } else {
      delete array[to];
    }
    from = from + direction;
    to = to + direction;
    count--;
  }

  return array;
}

// ES6 draft 07-15-13, section 15.4.3.23
function ArrayFind(predicate /* thisArg */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "Array.prototype.find");

  var array = $toObject(this);
  var length = $toInteger(array.length);

  if (!IS_SPEC_FUNCTION(predicate)) {
    throw MakeTypeError(kCalledNonCallable, predicate);
  }

  var thisArg;
  if (%_ArgumentsLength() > 1) {
    thisArg = %_Arguments(1);
  }

  var needs_wrapper = false;
  if (IS_NULL(thisArg)) {
    if (%IsSloppyModeFunction(predicate)) thisArg = UNDEFINED;
  } else if (!IS_UNDEFINED(thisArg)) {
    needs_wrapper = SHOULD_CREATE_WRAPPER(predicate, thisArg);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      var newThisArg = needs_wrapper ? $toObject(thisArg) : thisArg;
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

  var array = $toObject(this);
  var length = $toInteger(array.length);

  if (!IS_SPEC_FUNCTION(predicate)) {
    throw MakeTypeError(kCalledNonCallable, predicate);
  }

  var thisArg;
  if (%_ArgumentsLength() > 1) {
    thisArg = %_Arguments(1);
  }

  var needs_wrapper = false;
  if (IS_NULL(thisArg)) {
    if (%IsSloppyModeFunction(predicate)) thisArg = UNDEFINED;
  } else if (!IS_UNDEFINED(thisArg)) {
    needs_wrapper = SHOULD_CREATE_WRAPPER(predicate, thisArg);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      var newThisArg = needs_wrapper ? $toObject(thisArg) : thisArg;
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

  var array = $toObject(this);
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

  if ((end - i) > 0 && $objectIsFrozen(array)) {
    throw MakeTypeError(kArrayFunctionsOnFrozen);
  }

  for (; i < end; i++)
    array[i] = value;
  return array;
}

// ES6, draft 10-14-14, section 22.1.2.1
function ArrayFrom(arrayLike, mapfn, receiver) {
  var items = $toObject(arrayLike);
  var mapping = !IS_UNDEFINED(mapfn);

  if (mapping) {
    if (!IS_SPEC_FUNCTION(mapfn)) {
      throw MakeTypeError(kCalledNonCallable, mapfn);
    } else if (%IsSloppyModeFunction(mapfn)) {
      if (IS_NULL(receiver)) {
        receiver = UNDEFINED;
      } else if (!IS_UNDEFINED(receiver)) {
        receiver = TO_OBJECT_INLINE(receiver);
      }
    }
  }

  var iterable = $getMethod(items, symbolIterator);
  var k;
  var result;
  var mappedValue;
  var nextValue;

  if (!IS_UNDEFINED(iterable)) {
    result = %IsConstructor(this) ? new this() : [];

    var iterator = $getIterator(items, iterable);

    k = 0;
    while (true) {
      var next = iterator.next();

      if (!IS_OBJECT(next)) {
        throw MakeTypeError(kIteratorResultNotAnObject, next);
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
    var len = $toLength(items.length);
    result = %IsConstructor(this) ? new this(len) : new GlobalArray(len);

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

$installConstants(GlobalSymbol, [
  // TODO(dslomov, caitp): Move to symbol.js when shipping
  "isConcatSpreadable", symbolIsConcatSpreadable
]);

%FunctionSetLength(ArrayCopyWithin, 2);
%FunctionSetLength(ArrayFrom, 1);

// Set up non-enumerable functions on the Array object.
$installFunctions(GlobalArray, DONT_ENUM, [
  "from", ArrayFrom,
  "of", ArrayOf
]);

// Set up the non-enumerable functions on the Array prototype object.
$installFunctions(GlobalArray.prototype, DONT_ENUM, [
  "copyWithin", ArrayCopyWithin,
  "find", ArrayFind,
  "findIndex", ArrayFindIndex,
  "fill", ArrayFill
]);

})
