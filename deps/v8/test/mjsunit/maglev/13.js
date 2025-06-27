// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(a) {
  while(true) {
    if(5 < ++a) return a;
  }
}

%PrepareFunctionForOptimization(f);
assertEquals(f(0), 6);

%OptimizeMaglevOnNextCall(f);
assertEquals(f(0), 6);
