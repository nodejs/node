// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(a) {
  a = a >>> 0;
  %_DeoptimizeNow();
  return a;
}

function f() {
  return g(-1);
}

%OptimizeFunctionOnNextCall(f);
assertEquals(4294967295, f());
