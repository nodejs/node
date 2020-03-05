// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function* foo() {
  __v_1 = foo.x;   // LdaNamedProperty
  for (;;) {
    try {
      yield;
      try {
        __v_0 == "object";
        __v_1[__getRandomProperty()]();
      } catch(e) {
        print();
      }
    } catch(e) {
      "Caught: " + e;
    }
    break;
  }
};

%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo();
