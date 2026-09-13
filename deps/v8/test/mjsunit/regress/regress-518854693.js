// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --private-field-bytecodes --allow-natives-syntax

class A {
  #field = 0;
  test() {
    return ++this.#field;
  }
}

let a = new A();
%PrepareFunctionForOptimization(a.test);
assertEquals(1, a.test());
%OptimizeFunctionOnNextCall(a.test);
assertEquals(2, a.test());
