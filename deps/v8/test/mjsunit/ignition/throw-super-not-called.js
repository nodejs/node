// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --allow-natives-syntax --no-always-opt

class A {
  constructor() { }
}
class B extends A {
  constructor(call_super) {
    if (call_super) {
      super();
    }
  }
}

test = new B(1);
assertThrows(() => {new B(0)}, ReferenceError);

%PrepareFunctionForOptimization(B);
test = new B(1);
test = new B(1);
%OptimizeFunctionOnNextCall(B);
test = new B(1);
assertOptimized(B);
// Check that hole checks are handled correctly in optimized code.
assertThrows(() => {new B(0)}, ReferenceError);
assertOptimized(B);
