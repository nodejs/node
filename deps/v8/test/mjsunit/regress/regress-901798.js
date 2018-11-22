// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a) {
  return (a >>> 1073741824) + -3;
}

assertEquals(-3, f(0));
assertEquals(-2, f(1));
%OptimizeFunctionOnNextCall(f);
assertEquals(4294967291, f(-2));
