// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class SuperClass {
}

class SubClass extends SuperClass {
  constructor() {
    super();
    this.doSomething();
  }
  doSomething() {
  }
}

%PrepareFunctionForOptimization(SubClass);
new SubClass();
new SubClass();
%OptimizeFunctionOnNextCall(SubClass);
new SubClass();
