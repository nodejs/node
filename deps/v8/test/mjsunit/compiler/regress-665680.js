// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {}

var invalidAsmFunction = (function() {
  "use asm";
  return function() {
    with (foo) foo();
  }
})();

invalidAsmFunction();
%OptimizeFunctionOnNextCall(invalidAsmFunction);
invalidAsmFunction();
