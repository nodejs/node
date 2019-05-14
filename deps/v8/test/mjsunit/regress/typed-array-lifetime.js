// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --typed-array-max-size-in-heap=1

var foo = (function () {
  var y = 0;
  return function foo(x) {
    // Needs to be an external array.
    var a = new Float64Array(32);
    a[0] = 42;
    y = x + 0.1;  // This has to allocate.
    return a[0];
  }
})();

foo(1);
foo(1);
%OptimizeFunctionOnNextCall(foo);
foo(1);
// Try to force a GC during allocation in above marked line.
for (var i = 0; i < 20; ++i) {
  %SetAllocationTimeout(i, i, false);
  assertEquals(42, foo(2));
}
