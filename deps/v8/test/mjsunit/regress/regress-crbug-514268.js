// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(a) {
  a.pop();
}
function foo(a) {
  assertEquals(2, a.length);
  var d;
  for (d in a) {
    bar(a);
  }
  // If this fails, bar was not called exactly once.
  assertEquals(1, a.length);
}

foo([1,2]);
foo([2,3]);
%OptimizeFunctionOnNextCall(foo);
foo([1,2]);
