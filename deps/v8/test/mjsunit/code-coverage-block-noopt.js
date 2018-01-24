// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt --harmony-async-iteration
// Flags: --no-opt
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
 {"start":0,"end":33,"count":16},
 {"start":50,"end":76,"count":8}]
);

// In contrast to the corresponding test in -opt.js, f is not optimized here
// and therefore reports its invocation count correctly.
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
[{"start":52,"end":153,"count":1},
 {"start":111,"end":121,"count":0}]
);


%DebugToggleBlockCoverage(false);
