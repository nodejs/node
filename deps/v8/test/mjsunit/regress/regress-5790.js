// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo(a) {
  "use strict";
  if (a) return arguments[1];
}

%PrepareFunctionForOptimization(foo);
foo(false);
foo(false);
%OptimizeFunctionOnNextCall(foo);
foo(true, 1);
%PrepareFunctionForOptimization(foo);
foo(true, 1);
%OptimizeFunctionOnNextCall(foo);
foo(false);
foo(true, 1);
assertOptimized(foo);
