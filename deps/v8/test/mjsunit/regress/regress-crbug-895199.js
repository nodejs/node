// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var a = new Array(2);
  a[0] = 23.1234;
  a[1] = 25.1234;
  %DeoptimizeNow();
  return a[2];
};
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
