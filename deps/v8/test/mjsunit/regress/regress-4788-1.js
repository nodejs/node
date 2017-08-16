// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var f = (function() {
  "use asm";
  function foo(x) {
    return x == 0;
  }
  return foo;
})();

function deopt(f) {
  return {
    toString : function() {
      %DeoptimizeFunction(f);
      return "2";
    }
  };
}

%OptimizeFunctionOnNextCall(f);
assertFalse(f(deopt(f)));
