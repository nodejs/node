// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Expects following symbols to be set in the bootstrapper during genesis:
// - symbolHasInstance
// - symbolIsConcatSpreadable
// - symbolIsRegExp
// - symbolIterator
// - symbolToStringTag
// - symbolUnscopables

var $symbolToString;

(function() {

"use strict";

%CheckIsBootstrapping();

var GlobalArray = global.Array;
var GlobalObject = global.Object;
var GlobalSymbol = global.Symbol;

// -------------------------------------------------------------------

function SymbolConstructor(x) {
  if (%_IsConstructCall()) {
    throw MakeTypeError('not_constructor', ["Symbol"]);
  }
  // NOTE: Passing in a Symbol value will throw on ToString().
  return %CreateSymbol(IS_UNDEFINED(x) ? x : ToString(x));
}


function SymbolToString() {
  if (!(IS_SYMBOL(this) || IS_SYMBOL_WRAPPER(this))) {
    throw MakeTypeError(
      'incompatible_method_receiver', ["Symbol.prototype.toString", this]);
  }
  var description = %SymbolDescription(%_ValueOf(this));
  return "Symbol(" + (IS_UNDEFINED(description) ? "" : description) + ")";
}


function SymbolValueOf() {
  if (!(IS_SYMBOL(this) || IS_SYMBOL_WRAPPER(this))) {
    throw MakeTypeError(
      'incompatible_method_receiver', ["Symbol.prototype.valueOf", this]);
  }
  return %_ValueOf(this);
}


function SymbolFor(key) {
  key = TO_STRING_INLINE(key);
  var registry = %SymbolRegistry();
  if (IS_UNDEFINED(registry.for[key])) {
    var symbol = %CreateSymbol(key);
    registry.for[key] = symbol;
    registry.keyFor[symbol] = key;
  }
  return registry.for[key];
}


function SymbolKeyFor(symbol) {
  if (!IS_SYMBOL(symbol)) throw MakeTypeError("not_a_symbol", [symbol]);
  return %SymbolRegistry().keyFor[symbol];
}


// ES6 19.1.2.8
function ObjectGetOwnPropertySymbols(obj) {
  obj = ToObject(obj);

  // TODO(arv): Proxies use a shared trap for String and Symbol keys.

  return ObjectGetOwnPropertyKeys(obj, PROPERTY_ATTRIBUTES_STRING);
}

//-------------------------------------------------------------------

%SetCode(GlobalSymbol, SymbolConstructor);
%FunctionSetPrototype(GlobalSymbol, new GlobalObject());

InstallConstants(GlobalSymbol, GlobalArray(
  // TODO(rossberg): expose when implemented.
  // "hasInstance", symbolHasInstance,
  // "isConcatSpreadable", symbolIsConcatSpreadable,
  // "isRegExp", symbolIsRegExp,
  "iterator", symbolIterator,
  // TODO(dslomov, caitp): Currently defined in harmony-tostring.js ---
  // Move here when shipping
  // "toStringTag", symbolToStringTag,
  "unscopables", symbolUnscopables
));

InstallFunctions(GlobalSymbol, DONT_ENUM, GlobalArray(
  "for", SymbolFor,
  "keyFor", SymbolKeyFor
));

%AddNamedProperty(
    GlobalSymbol.prototype, "constructor", GlobalSymbol, DONT_ENUM);
%AddNamedProperty(
    GlobalSymbol.prototype, symbolToStringTag, "Symbol", DONT_ENUM | READ_ONLY);

InstallFunctions(GlobalSymbol.prototype, DONT_ENUM, GlobalArray(
  "toString", SymbolToString,
  "valueOf", SymbolValueOf
));

InstallFunctions(GlobalObject, DONT_ENUM, GlobalArray(
  "getOwnPropertySymbols", ObjectGetOwnPropertySymbols
));

$symbolToString = SymbolToString;

})();
