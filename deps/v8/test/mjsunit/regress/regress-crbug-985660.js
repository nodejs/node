// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
  Object.defineProperty(Number.prototype, "v", {
    get: constructor
  });
} catch (e) {}

function foo(obj) {
  return obj.v;
}

%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo(3);
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo(3);
foo(4);
