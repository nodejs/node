// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(s, t, u, v) {
  %DeoptimizeFunction(bar);
  return baz();
}
function bar() {
  return foo(11);
}
function baz() {
  return foo.arguments.length == 1 && foo.arguments[0] == 11;
}

%OptimizeFunctionOnNextCall(bar);
assertEquals(true, bar(12, 14));
