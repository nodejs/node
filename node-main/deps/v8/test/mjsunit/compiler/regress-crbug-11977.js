// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(str) {
  return str.startsWith();
}

%PrepareFunctionForOptimization(f);
assertEquals(f('undefined'), true);
%OptimizeFunctionOnNextCall(f);
assertEquals(f('undefined'), true);
