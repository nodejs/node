// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax --no-always-turbofan

class A {
  constructor() { }
}
class B extends A {
  constructor(call_super) {
    super();
    if (call_super) {
      super();
    }
  }
}
// No feedback case
test = new B(0);
assertThrows(() => {new B(1)}, ReferenceError);

// Ensure Feedback
%PrepareFunctionForOptimization(B);
%EnsureFeedbackVectorForFunction(A);
test = new B(0);
test = new B(0);
assertThrows(() => {new B(1)}, ReferenceError);
assertThrows(() => {new B(1)}, ReferenceError);
%OptimizeFunctionOnNextCall(B);
test = new B(0);
assertOptimized(B);
// Check that hole checks are handled correctly in optimized code.
assertThrows(() => {new B(1)}, ReferenceError);
assertOptimized(B);
