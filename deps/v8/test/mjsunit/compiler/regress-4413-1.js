// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var foo = (function(stdlib) {
  "use asm";
  var bar = stdlib.Symbol;
  function foo() { return bar("lala"); }
  return foo;
})(this);

%OptimizeFunctionOnNextCall(foo);
foo();
