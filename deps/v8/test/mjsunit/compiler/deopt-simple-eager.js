// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

// Simple eager deoptimization test.

function f(o) {
  return o.x;
}

assertEquals(f({x : 2}), 2);
assertEquals(f({x : 2}), 2);
%OptimizeFunctionOnNextCall(f);
assertEquals(f({x : 2}), 2);
assertEquals(f({y : 1, x  : 3}), 3);
