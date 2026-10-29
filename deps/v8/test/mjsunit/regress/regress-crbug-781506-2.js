// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(o) {
  return o[0];
};
%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo({}));
Array.prototype[0] = 0;
assertEquals(undefined, foo({}));
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo({}));
