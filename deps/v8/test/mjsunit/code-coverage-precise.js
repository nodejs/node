// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan --no-stress-flush-code
// Flags: --no-stress-incremental-marking
// Flags: --expose-gc
// Files: test/mjsunit/code-coverage-utils.js

// Test precise code coverage.

(async function () {

  // Without precise coverage enabled, we lose coverage data to the GC.
  await TestCoverage(
    "call an IIFE",
    `
(function f() {})();
    `,
    undefined  // The IIFE has been garbage-collected.
  );

  await TestCoverage(
    "call locally allocated function",
    `
for (var i = 0; i < 10; i++) {
  let f = () => 1;
  i += f();
}
    `,
    undefined
  );

  // This does not happen with precise coverage enabled.
  %DebugTogglePreciseCoverage(true);

  await TestCoverage(
    "call an IIFE",
    `
(function f() {})();
    `,
    [ {"start":0,"end":20,"count":1},
      {"start":1,"end":16,"count":1} ]
  );

  await TestCoverage(
    "call locally allocated function",
    `
for (var i = 0; i < 10; i++) {
  let f = () => 1;
  i += f();
}
    `,
    [ {"start":0,"end":63,"count":1},
      {"start":41,"end":48,"count":5} ]
  );

  await TestCoverage(
    "https://crbug.com/927464",
    `
!function f() {                           // 0000
  function unused() { nop(); }            // 0100
  nop();                                  // 0150
}();                                      // 0200
    `,
    [ {"start":0,"end":199,"count":1},
      {"start":1,"end":151,"count":1},
      {"start":52,"end":80,"count":0} ]
  );

  %DebugTogglePreciseCoverage(false);

})();
