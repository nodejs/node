// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class C5 extends null {
  constructor() {
    return -100000000000.0;
  }
}

function g() {
  try {
    new C5();
  } catch {
    return 1;
  }
  return 2;
}

%PrepareFunctionForOptimization(g);
%PrepareFunctionForOptimization(C5);
assertEquals(g(), 1);
%OptimizeFunctionOnNextCall(g);
assertEquals(g(), 1);
