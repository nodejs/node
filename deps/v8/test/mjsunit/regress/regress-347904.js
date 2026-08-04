// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-runs=2

var v = /abc/;
function f() {
  v = 1578221999;
};
%PrepareFunctionForOptimization(f);
;
%OptimizeFunctionOnNextCall(f);
f();
