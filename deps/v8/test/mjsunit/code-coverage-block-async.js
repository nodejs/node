// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt --no-stress-flush-bytecode
// Flags: --no-stress-incremental-marking
// Files: test/mjsunit/code-coverage-utils.js

%DebugToggleBlockCoverage(true);

TestCoverage(
"await expressions",
`
async function f() {                      // 0000
  await 42;                               // 0050
  await 42;                               // 0100
};                                        // 0150
f();                                      // 0200
%PerformMicrotaskCheckpoint();            // 0250
`,
[{"start":0,"end":299,"count":1},
  {"start":0,"end":151,"count":1}]
);

TestCoverage(
"for-await-of statements",
`
!async function() {                       // 0000
  for await (var x of [0,1,2,3]) {        // 0050
    nop();                                // 0100
  }                                       // 0150
}();                                      // 0200
%PerformMicrotaskCheckpoint();            // 0250
`,
[{"start":0,"end":299,"count":1},
  {"start":1,"end":201,"count":1},
  {"start":83,"end":153,"count":4}]
);

TestCoverage(
"https://crbug.com/981313",
`
class Foo {                               // 0000
  async timeout() {                       // 0000
    return new Promise(                   // 0100
        (r) => setTimeout(r, 10));        // 0000
  }                                       // 0200
}                                         // 0000
new Foo().timeout();                      // 0300
`,
[ {"start":0,  "end":349, "count":1},
  {"start":52, "end":203, "count":1},
  {"start":158,"end":182, "count":1}]);

TestCoverage(
  "test async generator coverage",
`
class Foo {                               // 0000
  async *timeout() {                      // 0000
    return new Promise(                   // 0100
        (r) => setTimeout(r, 10));        // 0000
  }                                       // 0200
}                                         // 0000
new Foo().timeout();                      // 0300
`,
  [ {"start":0,  "end":349, "count":1},
    {"start":52, "end":203, "count":1},
    {"start":158,"end":182, "count":0}]);

TestCoverage(
  "test async generator coverage with next call",
`
class Foo {                               // 0000
  async *timeout() {                      // 0000
    return new Promise(                   // 0100
        (r) => setTimeout(r, 10));        // 0000
  }                                       // 0200
}                                         // 0000
new Foo().timeout().next();               // 0300
`,
  [ {"start":0,  "end":349, "count":1},
    {"start":52, "end":203, "count":1},
    {"start":158,"end":182, "count":1}]);

TestCoverage(
  "test two consecutive returns",
`
class Foo {                               // 0000
  timeout() {                             // 0000
    return new Promise(                   // 0100
        (r) => setTimeout(r, 10));        // 0000
    return new Promise(                   // 0200
        (r) => setTimeout(r, 10));        // 0000
  }                                       // 0300
}                                         // 0000
new Foo().timeout();                      // 0400
`,
[ {"start":0,"end":449,"count":1},
  {"start":52,"end":303,"count":1},
  {"start":184,"end":302,"count":0},
  {"start":158,"end":182,"count":1}] );


TestCoverage(
  "test async generator with two consecutive returns",
`
class Foo {                               // 0000
  async *timeout() {                      // 0000
    return new Promise(                   // 0100
        (r) => setTimeout(r, 10));        // 0000
    return new Promise(                   // 0200
        (r) => setTimeout(r, 10));        // 0000
  }                                       // 0300
}                                         // 0000
new Foo().timeout().next();               // 0400
`,
[ {"start":0,"end":449,"count":1},
  {"start":52,"end":303,"count":1},
  {"start":184,"end":302,"count":0},
  {"start":158,"end":182,"count":1}] );

%DebugToggleBlockCoverage(false);
