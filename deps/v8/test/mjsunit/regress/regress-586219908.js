// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --stress-flush-code
// Flags: --no-flush-bytecode --always-sparkplug --stress-lazy

(function() {
  function f() {}
  %SetAllocationTimeout(1, 1);
  %CompileBaseline(f);
  %SetAllocationTimeout(-1, 0);
})();

(function() {
  function runEval() {
    return eval('(function g() {})');
  }
  let g1 = runEval();
  gc();
  let g2 = runEval();
  %CompileBaseline(g1);
  %SetAllocationTimeout(1, 1);
  %CompileBaseline(g2);
  %SetAllocationTimeout(-1, 0);
})();
