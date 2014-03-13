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

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

var $Symbol = global.Symbol;

// -------------------------------------------------------------------

function SymbolConstructor(x) {
  var value =
    IS_SYMBOL(x) ? x : %CreateSymbol(IS_UNDEFINED(x) ? x : ToString(x));
  if (%_IsConstructCall()) {
    %_SetValueOf(this, value);
  } else {
    return value;
  }
}

function SymbolGetName() {
  var symbol = IS_SYMBOL_WRAPPER(this) ? %_ValueOf(this) : this;
  if (!IS_SYMBOL(symbol)) {
    throw MakeTypeError(
        'incompatible_method_receiver', ["Symbol.prototype.name", this]);
  }
  return %SymbolName(symbol);
}

function SymbolToString() {
  throw MakeTypeError('symbol_to_string');
}

function SymbolValueOf() {
  // NOTE: Both Symbol objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_SYMBOL(this) && !IS_SYMBOL_WRAPPER(this)) {
    throw MakeTypeError(
        'incompatible_method_receiver', ["Symbol.prototype.valueOf", this]);
  }
  return %_ValueOf(this);
}


// ES6 19.1.2.8
function ObjectGetOwnPropertySymbols(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object",
                        ["Object.getOwnPropertySymbols"]);
  }

  // TODO(arv): Proxies use a shared trap for String and Symbol keys.

  return ObjectGetOwnPropertyKeys(obj, true);
}


//-------------------------------------------------------------------

function SetUpSymbol() {
  %CheckIsBootstrapping();

  %SetCode($Symbol, SymbolConstructor);
  %FunctionSetPrototype($Symbol, new $Symbol());
  %SetProperty($Symbol.prototype, "constructor", $Symbol, DONT_ENUM);

  InstallGetter($Symbol.prototype, "name", SymbolGetName);
  InstallFunctions($Symbol.prototype, DONT_ENUM, $Array(
    "toString", SymbolToString,
    "valueOf", SymbolValueOf
  ));
}

SetUpSymbol();


function ExtendObject() {
  %CheckIsBootstrapping();

  InstallFunctions($Object, DONT_ENUM, $Array(
    "getOwnPropertySymbols", ObjectGetOwnPropertySymbols
  ));
}

ExtendObject();
