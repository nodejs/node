// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var sentinelObject = {};
var evaluatedArg = false;
class C extends Object {
  constructor() {
    try {
      super(evaluatedArg = true);
    } catch (e) {
      assertInstanceof(e, TypeError);
      return sentinelObject;
    }
  }
}

%PrepareFunctionForOptimization(C);
Object.setPrototypeOf(C, parseInt);
assertSame(sentinelObject, new C());
assertSame(sentinelObject, new C());
%OptimizeFunctionOnNextCall(C)
assertSame(sentinelObject, new C());
assertFalse(evaluatedArg);
