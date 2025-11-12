// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo() {
      var a = { x: 2 };
      var confused = 0;
      confused += (a.x < 6);
      confused = (confused-1)
      return confused
}

%PrepareFunctionForOptimization(foo);
print(foo());
print(foo());
%OptimizeFunctionOnNextCall(foo);
print(foo());
