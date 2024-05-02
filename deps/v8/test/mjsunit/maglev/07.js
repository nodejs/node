// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(i) {
  var x = 1;
  if (i) { x = 2 }
  return x;
}

%PrepareFunctionForOptimization(f);
assertEquals(2, f(true));
assertEquals(1, f(false));

%OptimizeMaglevOnNextCall(f);
assertEquals(2, f(true));
assertEquals(1, f(false));
