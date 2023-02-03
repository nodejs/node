// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan --no-stress-flush-code
// Files: test/mjsunit/code-coverage-utils.js

// Test code coverage without explicitly activating it upfront.

TestCoverageNoGC(
"call simple function twice",
`
function f() {}
f();
f();
`,
[{"start":0,"end":25,"count":1},
 {"start":0,"end":15,"count":1}]
);

TestCoverageNoGC(
"call arrow function twice",
`
var f = () => 1;
f();
f();
`,
[{"start":0,"end":26,"count":1},
 {"start":8,"end":15,"count":1}]
);

TestCoverageNoGC(
"call nested function",
`
function f() {
  function g() {}
  g();
  g();
}
f();
f();
`,
[{"start":0,"end":58,"count":1},
 {"start":0,"end":48,"count":1},
 {"start":17,"end":32,"count":1}]
);

TestCoverageNoGC(
"call recursive function",
`
function fib(x) {
  if (x < 2) return 1;
  return fib(x-1) + fib(x-2);
}
fib(5);
`,
[{"start":0,"end":80,"count":1},
 {"start":0,"end":72,"count":1}]
);

TestCoverageNoGC(
"https://crbug.com/927464",
`
!function f() {                           // 0000
  function unused() { nop(); }            // 0100
  nop();                                  // 0150
}();                                      // 0200
`,
[{"start":0,"end":199,"count":1},
 {"start":1,"end":151,"count":1}
 // The unused function is unfortunately not marked as unused in best-effort
 // code coverage, as the information about its source range is discarded
 // entirely.
]
);
