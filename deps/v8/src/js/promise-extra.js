// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

var GlobalPromise = global.Promise;

var PromiseChain = utils.ImportNow("PromiseChain");
var PromiseDefer = utils.ImportNow("PromiseDefer");
var PromiseAccept = utils.ImportNow("PromiseAccept");

utils.InstallFunctions(GlobalPromise.prototype, DONT_ENUM, [
  "chain", PromiseChain,
]);

utils.InstallFunctions(GlobalPromise, DONT_ENUM, [
  "defer", PromiseDefer,
  "accept", PromiseAccept,
]);

})
