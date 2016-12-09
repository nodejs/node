// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalSymbol = global.Symbol;
var hasInstanceSymbol = utils.ImportNow("has_instance_symbol");
var isConcatSpreadableSymbol =
    utils.ImportNow("is_concat_spreadable_symbol");
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var matchSymbol = utils.ImportNow("match_symbol");
var replaceSymbol = utils.ImportNow("replace_symbol");
var searchSymbol = utils.ImportNow("search_symbol");
var speciesSymbol = utils.ImportNow("species_symbol");
var splitSymbol = utils.ImportNow("split_symbol");
var toPrimitiveSymbol = utils.ImportNow("to_primitive_symbol");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");
var unscopablesSymbol = utils.ImportNow("unscopables_symbol");

// -------------------------------------------------------------------

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
  if (!IS_SYMBOL(symbol)) throw %make_type_error(kSymbolKeyFor, symbol);
  return %SymbolRegistry().keyFor[symbol];
}

// -------------------------------------------------------------------

utils.InstallConstants(GlobalSymbol, [
  "hasInstance", hasInstanceSymbol,
  "isConcatSpreadable", isConcatSpreadableSymbol,
  "iterator", iteratorSymbol,
  "match", matchSymbol,
  "replace", replaceSymbol,
  "search", searchSymbol,
  "species", speciesSymbol,
  "split", splitSymbol,
  "toPrimitive", toPrimitiveSymbol,
  "toStringTag", toStringTagSymbol,
  "unscopables", unscopablesSymbol,
]);

utils.InstallFunctions(GlobalSymbol, DONT_ENUM, [
  "for", SymbolFor,
  "keyFor", SymbolKeyFor
]);

})
