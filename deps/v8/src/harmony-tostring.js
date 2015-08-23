// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var GlobalSymbol = global.Symbol;

utils.InstallConstants(GlobalSymbol, [
   // TODO(dslomov, caitp): Move to symbol.js when shipping
   "toStringTag", symbolToStringTag
]);

})
