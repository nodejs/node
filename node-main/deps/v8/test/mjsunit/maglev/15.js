// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

var xyz = 42;

function f(x) {
  return x < x;
}

%PrepareFunctionForOptimization(f);
assertEquals(f(1), false);

%OptimizeMaglevOnNextCall(f);
assertEquals(f(1), false);
