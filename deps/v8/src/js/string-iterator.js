// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalString = global.String;
var IteratorPrototype = utils.ImportNow("IteratorPrototype");
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var MakeTypeError;
var stringIteratorIteratedStringSymbol =
    utils.ImportNow("string_iterator_iterated_string_symbol");
var stringIteratorNextIndexSymbol =
    utils.ImportNow("string_iterator_next_index_symbol");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

function StringIterator() {}


// 21.1.5.1 CreateStringIterator Abstract Operation
function CreateStringIterator(string) {
  var s = TO_STRING(string);
  var iterator = new StringIterator;
  SET_PRIVATE(iterator, stringIteratorIteratedStringSymbol, s);
  SET_PRIVATE(iterator, stringIteratorNextIndexSymbol, 0);
  return iterator;
}


// ES6 section 21.1.5.2.1 %StringIteratorPrototype%.next ( )
function StringIteratorNext() {
  var iterator = this;
  var value = UNDEFINED;
  var done = true;

  if (!IS_RECEIVER(iterator) ||
      !HAS_DEFINED_PRIVATE(iterator, stringIteratorNextIndexSymbol)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'String Iterator.prototype.next');
  }

  var s = GET_PRIVATE(iterator, stringIteratorIteratedStringSymbol);
  if (!IS_UNDEFINED(s)) {
    var position = GET_PRIVATE(iterator, stringIteratorNextIndexSymbol);
    var length = TO_UINT32(s.length);
    if (position >= length) {
      SET_PRIVATE(iterator, stringIteratorIteratedStringSymbol, UNDEFINED);
    } else {
      var first = %_StringCharCodeAt(s, position);
      value = %_StringCharFromCode(first);
      done = false;
      position++;

      if (first >= 0xD800 && first <= 0xDBFF && position < length) {
        var second = %_StringCharCodeAt(s, position);
        if (second >= 0xDC00 && second <= 0xDFFF) {
          value += %_StringCharFromCode(second);
          position++;
        }
      }

      SET_PRIVATE(iterator, stringIteratorNextIndexSymbol, position);
    }
  }
  return %_CreateIterResultObject(value, done);
}


// 21.1.3.27 String.prototype [ @@iterator ]( )
function StringPrototypeIterator() {
  return CreateStringIterator(this);
}

//-------------------------------------------------------------------

%FunctionSetPrototype(StringIterator, {__proto__: IteratorPrototype});
%FunctionSetInstanceClassName(StringIterator, 'String Iterator');

utils.InstallFunctions(StringIterator.prototype, DONT_ENUM, [
  'next', StringIteratorNext
]);
%AddNamedProperty(StringIterator.prototype, toStringTagSymbol,
                  "String Iterator", READ_ONLY | DONT_ENUM);

utils.SetFunctionName(StringPrototypeIterator, iteratorSymbol);
%AddNamedProperty(GlobalString.prototype, iteratorSymbol,
                  StringPrototypeIterator, DONT_ENUM);

})
