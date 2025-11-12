// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [0, ""];
a[0] = 0;

function g(array) {
  array[1] = undefined;
}

function f() {
  g(function() {});
  g(a);
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
