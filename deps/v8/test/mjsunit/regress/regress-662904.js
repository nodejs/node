// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a) {
  var sum = 0;
  var a = [0, "a"];
  for (var i in a) {
    sum += a[i];
  }
  return sum;
}

%PrepareFunctionForOptimization(foo);
assertEquals("0a", foo());
assertEquals("0a", foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals("0a", foo());
