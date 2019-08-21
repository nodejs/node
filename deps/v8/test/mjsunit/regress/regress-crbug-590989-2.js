// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  return x === +x;
};
%PrepareFunctionForOptimization(f);
assertEquals(false, f(undefined));
assertEquals(false, f(undefined));
%OptimizeFunctionOnNextCall(f);
assertEquals(false, f(undefined));  // Interestingly this fails right away.
