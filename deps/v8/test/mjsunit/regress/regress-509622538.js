// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic --no-lazy-feedback-allocation

function makeSubclass() {
  class SuperClass {}
  SuperClass.prototype.foo = 123;
  class SubClass extends SuperClass {
    bar() {
      return super.foo;
    }
  }
  return SubClass;
}

let subClass = makeSubclass();
%PrepareFunctionForOptimization(subClass.prototype.bar);

// Run 6 times to collect homomorphic feedback (> 4 maps with same handler)
// in the shared feedback vector.
for (let i = 0; i < 6; ++i) {
  subClass = makeSubclass()
  let instance = new subClass();
  instance.bar();
}

%OptimizeFunctionOnNextCall(subClass.prototype.bar);
assertEquals(123, new subClass().bar());
