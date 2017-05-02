// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --function-context-specialization

function f(n) {
  var a = [];
  function g() { return x }
  for (var i = 0; i < n; ++i) {
    var x = i;
    a[i] = g;
    %OptimizeFunctionOnNextCall(g);
    g();
  }
  return a;
}
var a = f(3);
assertEquals(3, a.length);
assertEquals(2, a[0]());
assertEquals(2, a[1]());
assertEquals(2, a[2]());
