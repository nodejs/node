// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils, extrasUtils) {

"use strict";

%CheckIsBootstrapping();

// -----------------------------------------------------------------------
// Utils

var imports = UNDEFINED;
var exports_container = %ExportFromRuntime({});

// Export to other scripts.
function Export(f) {
  f(exports_container);
}


// Import from other scripts. The actual importing happens in PostNatives so
// that we can import from scripts executed later. However, that means that
// the import is not available until the very end. If the import needs to be
// available immediately, use ImportNow.
function Import(f) {
  f.next = imports;
  imports = f;
}


// Import immediately from exports of previous scripts. We need this for
// functions called during bootstrapping. Hooking up imports in PostNatives
// would be too late.
function ImportNow(name) {
  return exports_container[name];
}


// Prevents changes to the prototype of a built-in function.
// The "prototype" property of the function object is made non-configurable,
// and the prototype object is made non-extensible. The latter prevents
// changing the __proto__ property.
function SetUpLockedPrototype(
    constructor, fields, methods) {
  %CheckIsBootstrapping();
  var prototype = constructor.prototype;
  // Install functions first, because this function is used to initialize
  // PropertyDescriptor itself.
  var property_count = (methods.length >> 1) + (fields ? fields.length : 0);
  if (property_count >= 4) {
    %OptimizeObjectForAddingMultipleProperties(prototype, property_count);
  }
  if (fields) {
    for (var i = 0; i < fields.length; i++) {
      %AddNamedProperty(prototype, fields[i],
                        UNDEFINED, DONT_ENUM | DONT_DELETE);
    }
  }
  for (var i = 0; i < methods.length; i += 2) {
    var key = methods[i];
    var f = methods[i + 1];
    %AddNamedProperty(prototype, key, f, DONT_ENUM | DONT_DELETE | READ_ONLY);
    %SetNativeFlag(f);
  }
  %InternalSetPrototype(prototype, null);
  %ToFastProperties(prototype);
}

var GlobalArray = global.Array;
var InternalArray;

// -----------------------------------------------------------------------
// To be called by bootstrapper

function PostNatives(utils) {
  %CheckIsBootstrapping();

  // -------------------------------------------------------------------
  // Array

  InternalArray = utils.InternalArray;
  var iteratorSymbol = ImportNow("iterator_symbol");
  var unscopablesSymbol = ImportNow("unscopables_symbol");

  // Set up unscopable properties on the Array.prototype object.
  var unscopables = {
    __proto__: null,
    copyWithin: true,
    entries: true,
    fill: true,
    find: true,
    findIndex: true,
    includes: true,
    keys: true,
  };

  %ToFastProperties(unscopables);

  %AddNamedProperty(GlobalArray.prototype, unscopablesSymbol, unscopables,
                    DONT_ENUM | READ_ONLY);

  var ArrayIndexOf = GlobalArray.prototype.indexOf;
  var ArrayJoin = GlobalArray.prototype.join;
  var ArrayPop = GlobalArray.prototype.pop;
  var ArrayPush = GlobalArray.prototype.push;
  var ArraySlice = GlobalArray.prototype.slice;
  var ArrayShift = GlobalArray.prototype.shift;
  var ArraySort = GlobalArray.prototype.sort;
  var ArraySplice = GlobalArray.prototype.splice;
  var ArrayUnshift = GlobalArray.prototype.unshift;

  // Array prototype functions that return iterators. They are exposed to the
  // public API via Template::SetIntrinsicDataProperty().
  var ArrayEntries = GlobalArray.prototype.entries;
  var ArrayForEach = GlobalArray.prototype.forEach;
  var ArrayKeys = GlobalArray.prototype.keys;
  var ArrayValues = GlobalArray.prototype[iteratorSymbol];


  // The internal Array prototype doesn't need to be fancy, since it's never
  // exposed to user code.
  // Adding only the functions that are actually used.
  SetUpLockedPrototype(InternalArray, GlobalArray(), [
    "indexOf", ArrayIndexOf,
    "join", ArrayJoin,
    "pop", ArrayPop,
    "push", ArrayPush,
    "shift", ArrayShift,
    "sort", ArraySort,
    "splice", ArraySplice
  ]);

  // V8 extras get a separate copy of InternalPackedArray. We give them the basic
  // manipulation methods.
  SetUpLockedPrototype(extrasUtils.InternalPackedArray, GlobalArray(), [
    "push", ArrayPush,
    "pop", ArrayPop,
    "shift", ArrayShift,
    "unshift", ArrayUnshift,
    "splice", ArraySplice,
    "slice", ArraySlice
  ]);

  %InstallToContext([
    "array_entries_iterator", ArrayEntries,
    "array_for_each_iterator", ArrayForEach,
    "array_keys_iterator", ArrayKeys,
    "array_values_iterator", ArrayValues,
  ]);

  // -------------------------------------------------------------------

  for ( ; !IS_UNDEFINED(imports); imports = imports.next) {
    imports(exports_container);
  }

  exports_container = UNDEFINED;
  utils.Export = UNDEFINED;
  utils.Import = UNDEFINED;
  utils.ImportNow = UNDEFINED;
  utils.SetUpLockedPrototype = UNDEFINED;
  utils.PostNatives = UNDEFINED;
}

// -----------------------------------------------------------------------

%OptimizeObjectForAddingMultipleProperties(utils, 14);

utils.Import = Import;
utils.ImportNow = ImportNow;
utils.Export = Export;
utils.PostNatives = PostNatives;

%ToFastProperties(utils);

// -----------------------------------------------------------------------

%OptimizeObjectForAddingMultipleProperties(extrasUtils, 11);

extrasUtils.logStackTrace = function logStackTrace() {
  %DebugTrace();
};

extrasUtils.log = function log() {
  let message = '';
  for (const arg of arguments) {
    message += arg;
  }

  %GlobalPrint(message);
};

// Extras need the ability to store private state on their objects without
// exposing it to the outside world.

extrasUtils.createPrivateSymbol = function createPrivateSymbol(name) {
  return %CreatePrivateSymbol(name);
};

// These functions are key for safe meta-programming:
// http://wiki.ecmascript.org/doku.php?id=conventions:safe_meta_programming
//
// Technically they could all be derived from combinations of
// Function.prototype.{bind,call,apply} but that introduces lots of layers of
// indirection.

extrasUtils.uncurryThis = function uncurryThis(func) {
  return function(thisArg, ...args) {
    return %reflect_apply(func, thisArg, args);
  };
};

extrasUtils.markPromiseAsHandled = function markPromiseAsHandled(promise) {
  %PromiseMarkAsHandled(promise);
};

extrasUtils.promiseState = function promiseState(promise) {
  return %PromiseStatus(promise);
};

// [[PromiseState]] values (for extrasUtils.promiseState())
// These values should be kept in sync with PromiseStatus in globals.h
extrasUtils.kPROMISE_PENDING = 0;
extrasUtils.kPROMISE_FULFILLED = 1;
extrasUtils.kPROMISE_REJECTED = 2;

%ToFastProperties(extrasUtils);

})
