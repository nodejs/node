// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-slow-asserts

class Base {
  constructor() {
    this.x = 1;
  }
}

class Derived extends Base {
  constructor() {
    super();
  }
}

// Feed a bound function as new.target
// to the profiler, so HeapObjectMatcher
// can find it.
%PrepareFunctionForOptimization(Derived);
Reflect.construct(Derived, [], Object.bind());
%OptimizeFunctionOnNextCall(Derived);
new Derived();
