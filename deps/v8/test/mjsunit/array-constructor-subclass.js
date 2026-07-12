// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function A(C) {
  class D extends C {}
  return D;
};

class B extends A(Array) {
  constructor(x, y) { super(x); }
}

%PrepareFunctionForOptimization(B);
%PrepareFunctionForOptimization(A);
new B(2, 2);
%OptimizeFunctionOnNextCall(B);
new B(2, 2);
