// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(s) {
  %DeoptimizeFunction(bar);
  var x = 12;
  return s + x;
}

function bar(s, t) {
  return foo(s);
}

%OptimizeFunctionOnNextCall(bar);
assertEquals(13, bar(1, 2));
