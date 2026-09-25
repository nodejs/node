// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Object.prototype[1] = 1;
function foo(baz) {
  return 1 in arguments;
}
assertTrue(foo(0));
%PrepareFunctionForOptimization(foo);
assertTrue(foo(0));
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(0));
