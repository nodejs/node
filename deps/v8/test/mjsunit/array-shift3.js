// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Array.prototype[1] = "element 1";
function test(a) {
  a.shift();
  return a;
}
assertEquals(["element 1",{}], test([0,,{}]));
assertEquals(["element 1",10], test([9,,10]));
%OptimizeFunctionOnNextCall(test);
assertEquals(["element 1",10], test([9,,10]));
