// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalString = global.String;

var ArrayIteratorCreateResultObject;

utils.Import(function(from) {
  ArrayIteratorCreateResultObject = from.ArrayIteratorCreateResultObject;
});

// -------------------------------------------------------------------

var stringIteratorIteratedStringSymbol =
    GLOBAL_PRIVATE("StringIterator#iteratedString");
var stringIteratorNextIndexSymbol = GLOBAL_PRIVATE("StringIterator#next");


function StringIterator() {}


// 21.1.5.1 CreateStringIterator Abstract Operation
function CreateStringIterator(string) {
  var s = TO_STRING_INLINE(string);
  var iterator = new StringIterator;
  SET_PRIVATE(iterator, stringIteratorIteratedStringSymbol, s);
  SET_PRIVATE(iterator, stringIteratorNextIndexSymbol, 0);
  return iterator;
}


// 21.1.5.2.1 %StringIteratorPrototype%.next( )
function StringIteratorNext() {
  var iterator = $toObject(this);

  if (!HAS_DEFINED_PRIVATE(iterator, stringIteratorNextIndexSymbol)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'String Iterator.prototype.next');
  }

  var s = GET_PRIVATE(iterator, stringIteratorIteratedStringSymbol);
  if (IS_UNDEFINED(s)) {
    return ArrayIteratorCreateResultObject(UNDEFINED, true);
  }

  var position = GET_PRIVATE(iterator, stringIteratorNextIndexSymbol);
  var length = TO_UINT32(s.length);

  if (position >= length) {
    SET_PRIVATE(iterator, stringIteratorIteratedStringSymbol,
                UNDEFINED);
    return ArrayIteratorCreateResultObject(UNDEFINED, true);
  }

  var first = %_StringCharCodeAt(s, position);
  var resultString = %_StringCharFromCode(first);
  position++;

  if (first >= 0xD800 && first <= 0xDBFF && position < length) {
    var second = %_StringCharCodeAt(s, position);
    if (second >= 0xDC00 && second <= 0xDFFF) {
      resultString += %_StringCharFromCode(second);
      position++;
    }
  }

  SET_PRIVATE(iterator, stringIteratorNextIndexSymbol, position);

  return ArrayIteratorCreateResultObject(resultString, false);
}


// 21.1.3.27 String.prototype [ @@iterator ]( )
function StringPrototypeIterator() {
  return CreateStringIterator(this);
}

//-------------------------------------------------------------------

%FunctionSetPrototype(StringIterator, {__proto__: $iteratorPrototype});
%FunctionSetInstanceClassName(StringIterator, 'String Iterator');

utils.InstallFunctions(StringIterator.prototype, DONT_ENUM, [
  'next', StringIteratorNext
]);
%AddNamedProperty(StringIterator.prototype, symbolToStringTag,
                  "String Iterator", READ_ONLY | DONT_ENUM);

utils.SetFunctionName(StringPrototypeIterator, symbolIterator);
%AddNamedProperty(GlobalString.prototype, symbolIterator,
                  StringPrototypeIterator, DONT_ENUM);

})
