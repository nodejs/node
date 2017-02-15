// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(f) {
  f(0);
  f(NaN);
  %OptimizeFunctionOnNextCall(f);
  f(1.0);
}

test(x => Math.cosh(+x));
test(x => Math.sinh(+x));
test(x => Math.tanh(+x));
