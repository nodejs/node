// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --no-lazy-feedback-allocation --allow-natives-syntax

class Base extends Object {
  constructor() {
    return 1;
  }
}

class Derived extends Base {
  constructor() {
    try {
      super();
    } catch (e) {}
    try {
      super();
    } catch (e) {}
    return {};
  }
}

%PrepareFunctionForOptimization(Derived);
Reflect.construct(Derived, []);
%OptimizeFunctionOnNextCall(Derived);
const result = Reflect.construct(Derived, []);
assertInstanceof(result, Object);
