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


function InstallConstants(object, constants) {
  %CheckIsBootstrapping();
  %OptimizeObjectForAddingMultipleProperties(object, constants.length >> 1);
  var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;
  for (var i = 0; i < constants.length; i += 2) {
    var name = constants[i];
    var k = constants[i + 1];
    %AddNamedProperty(object, name, k, attributes);
  }
  %ToFastProperties(object);
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


// -----------------------------------------------------------------------
// To be called by bootstrapper

function PostNatives(utils) {
  %CheckIsBootstrapping();

  for ( ; !IS_UNDEFINED(imports); imports = imports.next) {
    imports(exports_container);
  }

  exports_container = UNDEFINED;
  utils.Export = UNDEFINED;
  utils.Import = UNDEFINED;
  utils.ImportNow = UNDEFINED;
  utils.PostNatives = UNDEFINED;
}

// -----------------------------------------------------------------------

%OptimizeObjectForAddingMultipleProperties(utils, 14);

utils.Import = Import;
utils.ImportNow = ImportNow;
utils.Export = Export;
utils.InstallConstants = InstallConstants;
utils.SetUpLockedPrototype = SetUpLockedPrototype;
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
// indirection and slowness given how un-optimized bind is.

extrasUtils.simpleBind = function simpleBind(func, thisArg) {
  return function(...args) {
    return %reflect_apply(func, thisArg, args);
  };
};

extrasUtils.uncurryThis = function uncurryThis(func) {
  return function(thisArg, ...args) {
    return %reflect_apply(func, thisArg, args);
  };
};

// We pass true to trigger the debugger's on exception handler.
extrasUtils.rejectPromise = function rejectPromise(promise, reason) {
  %promise_internal_reject(promise, reason, true);
}

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
