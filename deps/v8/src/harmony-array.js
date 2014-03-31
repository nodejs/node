// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  if (IS_NULL_OR_UNDEFINED(thisArg)) {
    thisArg = %GetDefaultReceiver(predicate) || thisArg;
  } else if (!IS_SPEC_OBJECT(thisArg) && %IsSloppyModeFunction(predicate)) {
    thisArg = ToObject(thisArg);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      if (%_CallFunction(thisArg, element, i, array, predicate)) {
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

  if (IS_NULL_OR_UNDEFINED(thisArg)) {
    thisArg = %GetDefaultReceiver(predicate) || thisArg;
  } else if (!IS_SPEC_OBJECT(thisArg) && %IsSloppyModeFunction(predicate)) {
    thisArg = ToObject(thisArg);
  }

  for (var i = 0; i < length; i++) {
    if (i in array) {
      var element = array[i];
      if (%_CallFunction(thisArg, element, i, array, predicate)) {
        return i;
      }
    }
  }

  return -1;
}


// -------------------------------------------------------------------

function HarmonyArrayExtendArrayPrototype() {
  %CheckIsBootstrapping();

  // Set up the non-enumerable functions on the Array prototype object.
  InstallFunctions($Array.prototype, DONT_ENUM, $Array(
    "find", ArrayFind,
    "findIndex", ArrayFindIndex
  ));
}

HarmonyArrayExtendArrayPrototype();
