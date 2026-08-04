// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(x) {
  let y = -9007199254740990;
  y++;
  return (0 + x) ** y;
}

const x = -1.0219695401608718e+308;
%PrepareFunctionForOptimization(test);
assertEquals(-0, test(x));
assertEquals(-0, test(x));
%OptimizeFunctionOnNextCall(test);
assertEquals(-0, test(x));
