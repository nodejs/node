// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class A {
  a() { this.#b() }
  #b() {}
}

function InlinePrivateMethod() {
  for (let i = 0; i < 10; i++) {
    new A().a();
  }
}

%PrepareFunctionForOptimization(A);
%PrepareFunctionForOptimization(InlinePrivateMethod);
InlinePrivateMethod();
%OptimizeFunctionOnNextCall(InlinePrivateMethod);
InlinePrivateMethod();
