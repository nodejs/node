// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports
//
var GlobalProxy = global.Proxy;
var MakeTypeError;

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

//----------------------------------------------------------------------------

function ProxyCreateRevocable(target, handler) {
  var p = new GlobalProxy(target, handler);
  return {proxy: p, revoke: () => %JSProxyRevoke(p)};
}

// -------------------------------------------------------------------
// Proxy Builtins

// Implements part of ES6 9.5.11 Proxy.[[Enumerate]]:
// Call the trap, which should return an iterator, exhaust the iterator,
// and return an array containing the values.
function ProxyEnumerate(trap, handler, target) {
  // 7. Let trapResult be ? Call(trap, handler, «target»).
  var trap_result = %_Call(trap, handler, target);
  // 8. If Type(trapResult) is not Object, throw a TypeError exception.
  if (!IS_RECEIVER(trap_result)) {
    throw MakeTypeError(kProxyEnumerateNonObject);
  }
  // 9. Return trapResult.
  var result = [];
  for (var it = trap_result.next(); !it.done; it = trap_result.next()) {
    var key = it.value;
    // Not yet spec'ed as of 2015-11-25, but will be spec'ed soon:
    // If the iterator returns a non-string value, throw a TypeError.
    if (!IS_STRING(key)) {
      throw MakeTypeError(kProxyEnumerateNonString);
    }
    result.push(key);
  }
  return result;
}

//-------------------------------------------------------------------

//Set up non-enumerable properties of the Proxy object.
utils.InstallFunctions(GlobalProxy, DONT_ENUM, [
  "revocable", ProxyCreateRevocable
]);

// -------------------------------------------------------------------
// Exports

%InstallToContext([
  "proxy_enumerate", ProxyEnumerate,
]);

})
