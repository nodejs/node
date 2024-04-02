// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --check-elimination --stress-opt

var v0 = this;
var v2 = this;
function f() {
  v2 = [1.2, 2.3];
  v0 = [12, 23];
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
