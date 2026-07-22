// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(s) {
  %DeoptimizeFunction(bar);
  return baz();
}
function bar() { return foo(13, 14, 15); }
function baz() {
  return foo.arguments.length == 3 &&
         foo.arguments[0] == 13 &&
         foo.arguments[1] == 14 &&
         foo.arguments[2] == 15;
}

%PrepareFunctionForOptimization(bar);
%OptimizeFunctionOnNextCall(bar);
assertEquals(true, bar(12, 14));
