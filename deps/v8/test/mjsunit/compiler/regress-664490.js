// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var foo = function(msg) {};

foo = function (value) {
  assertEquals(false, value);
}

function f() {
  foo(undefined == 0);
}

%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
f();
