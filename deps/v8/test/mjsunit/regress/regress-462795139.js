// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --assert-types --no-maglev-inlining

'use strict';

function callee(unused) {
  var x = {};
}

function caller() {
  callee(1);
}

%PrepareFunctionForOptimization(callee);
%PrepareFunctionForOptimization(caller);
caller();
caller();

%OptimizeFunctionOnNextCall(callee);
callee(1);

%OptimizeMaglevOnNextCall(caller);
caller();
