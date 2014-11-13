// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Array = global.Array;

var $Symbol = global.Symbol;

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


function InternalSymbol(key) {
  var internal_registry = %SymbolRegistry().for_intern;
  if (IS_UNDEFINED(internal_registry[key])) {
    internal_registry[key] = %CreateSymbol(key);
  }
  return internal_registry[key];
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

var symbolHasInstance = InternalSymbol("Symbol.hasInstance");
var symbolIsConcatSpreadable = InternalSymbol("Symbol.isConcatSpreadable");
var symbolIsRegExp = InternalSymbol("Symbol.isRegExp");
var symbolIterator = InternalSymbol("Symbol.iterator");
var symbolToStringTag = InternalSymbol("Symbol.toStringTag");
var symbolUnscopables = InternalSymbol("Symbol.unscopables");


//-------------------------------------------------------------------

function SetUpSymbol() {
  %CheckIsBootstrapping();

  %SetCode($Symbol, SymbolConstructor);
  %FunctionSetPrototype($Symbol, new $Object());

  InstallConstants($Symbol, $Array(
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
  InstallFunctions($Symbol, DONT_ENUM, $Array(
    "for", SymbolFor,
    "keyFor", SymbolKeyFor
  ));

  %AddNamedProperty($Symbol.prototype, "constructor", $Symbol, DONT_ENUM);
  %AddNamedProperty(
      $Symbol.prototype, symbolToStringTag, "Symbol", DONT_ENUM | READ_ONLY);
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
