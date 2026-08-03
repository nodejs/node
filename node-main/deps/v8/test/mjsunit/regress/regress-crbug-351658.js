// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

try {
  var f = eval("(function(){0 = y + y})");
  %OptimizeFunctionOnNextCall(f);
  f();
  assertUnreachable();
} catch(e) {
  assertTrue(e instanceof SyntaxError);
}
