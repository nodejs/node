// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var ObserveArrayMethods = utils.ImportNow("ObserveArrayMethods");
var ObserveObjectMethods = utils.ImportNow("ObserveObjectMethods");;

utils.InstallFunctions(global.Object, DONT_ENUM, ObserveObjectMethods);
utils.InstallFunctions(global.Array, DONT_ENUM, ObserveArrayMethods);

})
