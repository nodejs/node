// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;
var GlobalSymbol = global.Symbol;
var hasInstanceSymbol = utils.ImportNow("has_instance_symbol");
var isConcatSpreadableSymbol =
    utils.ImportNow("is_concat_spreadable_symbol");
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var MakeTypeError;
var toPrimitiveSymbol = utils.ImportNow("to_primitive_symbol");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");
var unscopablesSymbol = utils.ImportNow("unscopables_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

// 19.4.3.4 Symbol.prototype [ @@toPrimitive ] ( hint )
function SymbolToPrimitive(hint) {
  if (!(IS_SYMBOL(this) || IS_SYMBOL_WRAPPER(this))) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "Symbol.prototype [ @@toPrimitive ]", this);
  }
  return %_ValueOf(this);
}


function SymbolToString() {
  if (!(IS_SYMBOL(this) || IS_SYMBOL_WRAPPER(this))) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "Symbol.prototype.toString", this);
  }
  return %SymbolDescriptiveString(%_ValueOf(this));
}


function SymbolValueOf() {
  if (!(IS_SYMBOL(this) || IS_SYMBOL_WRAPPER(this))) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "Symbol.prototype.valueOf", this);
  }
  return %_ValueOf(this);
}


function SymbolFor(key) {
  key = TO_STRING(key);
  var registry = %SymbolRegistry();
  if (IS_UNDEFINED(registry.for[key])) {
    var symbol = %CreateSymbol(key);
    registry.for[key] = symbol;
    registry.keyFor[symbol] = key;
  }
  return registry.for[key];
}


function SymbolKeyFor(symbol) {
  if (!IS_SYMBOL(symbol)) throw MakeTypeError(kSymbolKeyFor, symbol);
  return %SymbolRegistry().keyFor[symbol];
}


// ES6 19.1.2.8
function ObjectGetOwnPropertySymbols(obj) {
  obj = TO_OBJECT(obj);

  return %GetOwnPropertyKeys(obj, PROPERTY_FILTER_SKIP_STRINGS);
}

// -------------------------------------------------------------------

%FunctionSetPrototype(GlobalSymbol, new GlobalObject());

utils.InstallConstants(GlobalSymbol, [
  // TODO(rossberg): expose when implemented.
  // "hasInstance", hasInstanceSymbol,
  // "isConcatSpreadable", isConcatSpreadableSymbol,
  "iterator", iteratorSymbol,
  // TODO(yangguo): expose when implemented.
  // "match", matchSymbol,
  // "replace", replaceSymbol,
  // "search", searchSymbol,
  // "split, splitSymbol,
  "toPrimitive", toPrimitiveSymbol,
  // TODO(dslomov, caitp): Currently defined in harmony-tostring.js ---
  // Move here when shipping
  // "toStringTag", toStringTagSymbol,
  "unscopables", unscopablesSymbol,
]);

utils.InstallFunctions(GlobalSymbol, DONT_ENUM, [
  "for", SymbolFor,
  "keyFor", SymbolKeyFor
]);

%AddNamedProperty(
    GlobalSymbol.prototype, "constructor", GlobalSymbol, DONT_ENUM);
%AddNamedProperty(
    GlobalSymbol.prototype, toStringTagSymbol, "Symbol", DONT_ENUM | READ_ONLY);

utils.InstallFunctions(GlobalSymbol.prototype, DONT_ENUM | READ_ONLY, [
  toPrimitiveSymbol, SymbolToPrimitive
]);

utils.InstallFunctions(GlobalSymbol.prototype, DONT_ENUM, [
  "toString", SymbolToString,
  "valueOf", SymbolValueOf
]);

utils.InstallFunctions(GlobalObject, DONT_ENUM, [
  "getOwnPropertySymbols", ObjectGetOwnPropertySymbols
]);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.SymbolToString = SymbolToString;
})

})
