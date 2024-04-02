// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, i) {
  a[i].x;
};
%PrepareFunctionForOptimization(foo);
var a = [, 0.1];
foo(a, 1);
foo(a, 1);
%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo(a, 0), TypeError);
