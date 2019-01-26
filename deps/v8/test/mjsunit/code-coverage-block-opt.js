// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt --opt
// Files: test/mjsunit/code-coverage-utils.js

%DebugToggleBlockCoverage(true);

TestCoverage(
"optimized and inlined functions",
`
function g() { if (true) nop(); }         // 0000
function f() { g(); g(); }                // 0050
f(); f(); %OptimizeFunctionOnNextCall(f); // 0100
f(); f(); f(); f(); f(); f();             // 0150
`,
[{"start":0,"end":199,"count":1},
 {"start":0,"end":33,"count":4},   // TODO(jgruber): Invocation count is off.
 {"start":25,"end":31,"count":16},
 {"start":50,"end":76,"count":2}]  // TODO(jgruber): Invocation count is off.
);

// This test is tricky: it requires a non-toplevel, optimized function.
// After initial collection, counts are cleared. Further invocation_counts
// are not collected for optimized functions, and on the next coverage
// collection we and up with an uncovered function with an uncovered parent
// but with non-trivial block coverage.
TestCoverage("Partial coverage collection",
`
!function() {                             // 0000
  function f(x) {                         // 0050
    if (x) { nop(); } else { nop(); }     // 0100
  }                                       // 0150
  f(true); f(true);                       // 0200
  %OptimizeFunctionOnNextCall(f);         // 0250
  %DebugCollectCoverage();                // 0300
  f(false);                               // 0350
}();                                      // 0400
`,
[{"start":52,"end":153,"count":0},
 {"start":121,"end":137,"count":1}]
);

%DebugToggleBlockCoverage(false);
