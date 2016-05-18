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

//----------------------------------------------------------------------------

function ProxyCreateRevocable(target, handler) {
  var p = new GlobalProxy(target, handler);
  return {proxy: p, revoke: () => %JSProxyRevoke(p)};
}

//-------------------------------------------------------------------

//Set up non-enumerable properties of the Proxy object.
utils.InstallFunctions(GlobalProxy, DONT_ENUM, [
  "revocable", ProxyCreateRevocable
]);

})
