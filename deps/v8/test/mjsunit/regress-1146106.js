// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --concurrent-inlining --no-use-ic --super-ic

class A {
  bar() { }
}
class B extends A {
  foo() {
    return super.bar();
  }
}
%PrepareFunctionForOptimization(B.prototype.foo);
new B().foo();
%OptimizeFunctionOnNextCall(B.prototype.foo);
new B().foo();
