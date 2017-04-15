// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(s) { return arguments; }
function bar(s, t) {
  var args = foo(s, t, 13);
  return args.length == 3 &&
         args[0] == 11 &&
         args[1] == 12 &&
         args[2] == 13;
}

%OptimizeFunctionOnNextCall(bar);
assertEquals(true, bar(11, 12));
