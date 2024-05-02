// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

function f(x) {
  return x;
}

%PrepareFunctionForOptimization(f);
// f(x) takes one argument but we are under-applying here
assertEquals(undefined, f());
// f(x) takes one argument but we are over-applying here
assertEquals(1, f(1, 2));

%OptimizeMaglevOnNextCall(f);
// f(x) takes one argument but we are under-applying here
assertEquals(undefined, f());
// f(x) takes one argument but we are over-applying here
assertEquals(1, f(1, 2));
