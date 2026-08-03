// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a) {
  a.x = 0;
  if (a.x === 0) a[1] = 0.1;
  a.x = {};
};
%PrepareFunctionForOptimization(foo);
foo(new Array(1));
foo(new Array(1));
%OptimizeFunctionOnNextCall(foo);
foo(new Array(1));
