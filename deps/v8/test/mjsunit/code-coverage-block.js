// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt
// Files: test/mjsunit/code-coverage-utils.js

%DebugToggleBlockCoverage(true);

TestCoverage(
"call an IIFE",
`
(function f() {})();
`,
[{"start":0,"end":20,"count":1},{"start":1,"end":16,"count":1}]
);

TestCoverage(
"call locally allocated function",
`let f = () => 1; f();`,
[{"start":0,"end":21,"count":1},{"start":8,"end":15,"count":1}]
);

TestCoverage(
"if statements",
`
function g() {}                           // 0000
function f(x) {                           // 0050
  if (x == 42) {                          // 0100
    if (x == 43) g(); else g();           // 0150
  }                                       // 0200
  if (x == 42) { g(); } else { g(); }     // 0250
  if (x == 42) g(); else g();             // 0300
  if (false) g(); else g();               // 0350
  if (false) g();                         // 0400
  if (true) g(); else g();                // 0450
  if (true) g();                          // 0500
}                                         // 0550
f(42);                                    // 0600
f(43);                                    // 0650
if (true) {                               // 0700
  const foo = 'bar';                      // 0750
} else {                                  // 0800
  const bar = 'foo';                      // 0850
}                                         // 0900
`,
[{"start":0,"end":949,"count":1},
 {"start":801,"end":901,"count":0},
 {"start":0,"end":15,"count":11},
 {"start":50,"end":551,"count":2},
 {"start":115,"end":203,"count":1},
 {"start":167,"end":171,"count":0},
 {"start":265,"end":287,"count":1},
 {"start":315,"end":329,"count":1},
 {"start":363,"end":367,"count":0},
 {"start":413,"end":417,"count":0},
 {"start":466,"end":476,"count":0}]
);

TestCoverage(
"if statement (early return)",
`
!function() {                             // 0000
  if (true) {                             // 0050
    nop();                                // 0100
    return;                               // 0150
    nop();                                // 0200
  }                                       // 0250
  nop();                                  // 0300
}()                                       // 0350
`,
[{"start":0,"end":399,"count":1},
 {"start":1,"end":351,"count":1},
 {"start":161,"end":350,"count":0}]
);

TestCoverage(
"if statement (no semi-colon)",
`
!function() {                             // 0000
  if (true) nop()                         // 0050
  if (true) nop(); else nop()             // 0100
  nop();                                  // 0150
}()                                       // 0200
`,
[{"start":0,"end":249,"count":1},
 {"start":1,"end":201,"count":1},
 {"start":118,"end":129,"count":0}]
);

TestCoverage(
"for statements",
`
function g() {}                           // 0000
!function() {                             // 0050
  for (var i = 0; i < 12; i++) g();       // 0100
  for (var i = 0; i < 12; i++) {          // 0150
    g();                                  // 0200
  }                                       // 0250
  for (var i = 0; false; i++) g();        // 0300
  for (var i = 0; true; i++) break;       // 0350
  for (var i = 0; i < 12; i++) {          // 0400
    if (i % 3 == 0) g(); else g();        // 0450
  }                                       // 0500
}();                                      // 0550
`,
[{"start":0,"end":599,"count":1},
 {"start":0,"end":15,"count":36},
 {"start":51,"end":551,"count":1},
 {"start":131,"end":135,"count":12},
 {"start":181,"end":253,"count":12},
 {"start":330,"end":334,"count":0},
 {"start":431,"end":503,"count":12},
 {"start":470,"end":474,"count":4},
 {"start":474,"end":484,"count":8}]
);

TestCoverage(
"for statements pt. 2",
`
function g() {}                           // 0000
!function() {                             // 0050
  let j = 0;                              // 0100
  for (let i = 0; i < 12; i++) g();       // 0150
  for (const i = 0; j < 12; j++) g();     // 0200
  for (j = 0; j < 12; j++) g();           // 0250
  for (;;) break;                         // 0300
}();                                      // 0350
`,
[{"start":0,"end":399,"count":1},
 {"start":0,"end":15,"count":36},
 {"start":51,"end":351,"count":1},
 {"start":181,"end":185,"count":12},
 {"start":233,"end":237,"count":12},
 {"start":277,"end":281,"count":12}]
);

TestCoverage(
"for statements (no semicolon)",
`
function g() {}                           // 0000
!function() {                             // 0050
  for (let i = 0; i < 12; i++) g()        // 0100
  for (let i = 0; i < 12; i++) break      // 0150
  for (let i = 0; i < 12; i++) break; g() // 0200
}();                                      // 0250
`,
[{"start":0,"end":299,"count":1},
 {"start":0,"end":15,"count":13},
 {"start":51,"end":251,"count":1},
 {"start":131,"end":134,"count":12}]
);

TestCoverage(
"for statement (early return)",
`
!function() {                             // 0000
  for (var i = 0; i < 10; i++) {          // 0050
    nop();                                // 0100
    continue;                             // 0150
    nop();                                // 0200
  }                                       // 0250
  nop();                                  // 0300
  for (;;) {                              // 0350
    nop();                                // 0400
    break;                                // 0450
    nop();                                // 0500
  }                                       // 0550
  nop();                                  // 0600
  for (;;) {                              // 0650
    nop();                                // 0700
    return;                               // 0750
    nop();                                // 0800
  }                                       // 0850
  nop();                                  // 0900
}()                                       // 0950
`,
[{"start":0,"end":999,"count":1},
 {"start":1,"end":951,"count":1},
 {"start":81,"end":253,"count":10},
 {"start":163,"end":253,"count":0},
 {"start":460,"end":553,"count":0},
 {"start":761,"end":950,"count":0}]
);

TestCoverage(
"for-of and for-in statements",
`
!function() {                             // 0000
  var i;                                  // 0050
  for (i of [0,1,2,3]) { nop(); }         // 0100
  for (let j of [0,1,2,3]) { nop(); }     // 0150
  for (i in [0,1,2,3]) { nop(); }         // 0200
  for (let j in [0,1,2,3]) { nop(); }     // 0250
  var xs = [{a:0, b:1}, {a:1,b:0}];       // 0300
  for (var {a: x, b: y} of xs) { nop(); } // 0350
}();                                      // 0400
`,
[{"start":0,"end":449,"count":1},
 {"start":1,"end":401,"count":1},
 {"start":123,"end":133,"count":4},
 {"start":177,"end":187,"count":4},
 {"start":223,"end":233,"count":4},
 {"start":277,"end":287,"count":4},
 {"start":381,"end":391,"count":2}]
);

TestCoverage(
"for-await-of statements",
`
!async function() {                       // 0000
  for await (var x of [0,1,2,3]) {        // 0050
    nop();                                // 0100
  }                                       // 0150
}();                                      // 0200
%RunMicrotasks();                         // 0250
`,
[{"start":0,"end":299,"count":1},
 {"start":1,"end":201,"count":6},  // TODO(jgruber): Invocation count is off.
 {"start":83,"end":153,"count":4},
 {"start":153,"end":200,"count":1}]
);

TestCoverage(
"while and do-while statements",
`
function g() {}                           // 0000
!function() {                             // 0050
  var i;                                  // 0100
  i = 0; while (i < 12) i++;              // 0150
  i = 0; while (i < 12) { g(); i++; }     // 0200
  i = 0; while (false) g();               // 0250
  i = 0; while (true) break;              // 0300
                                          // 0350
  i = 0; do i++; while (i < 12);          // 0400
  i = 0; do { g(); i++; }                 // 0450
         while (i < 12);                  // 0500
  i = 0; do { g(); } while (false);       // 0550
  i = 0; do { break; } while (true);      // 0600
}();                                      // 0650
`,
[{"start":0,"end":699,"count":1},
 {"start":0,"end":15,"count":25},
 {"start":51,"end":651,"count":1},
 {"start":174,"end":178,"count":12},
 {"start":224,"end":237,"count":12},
 {"start":273,"end":277,"count":0},
 {"start":412,"end":416,"count":12},
 {"start":462,"end":475,"count":12},
 {"start":620,"end":622,"count":0}]
);

TestCoverage(
"while statement (early return)",
`
!function() {                             // 0000
  let i = 0;                              // 0050
  while (i < 10) {                        // 0100
    i++;                                  // 0150
    continue;                             // 0200
    nop();                                // 0250
  }                                       // 0300
  nop();                                  // 0350
  while (true) {                          // 0400
    nop();                                // 0450
    break;                                // 0500
    nop();                                // 0550
  }                                       // 0600
  nop();                                  // 0650
  while (true) {                          // 0700
    nop();                                // 0750
    return;                               // 0800
    nop();                                // 0850
  }                                       // 0900
  nop();                                  // 0950
}()                                       // 1000
`,
[{"start":0,"end":1049,"count":1},
 {"start":1,"end":1001,"count":1},
 {"start":117,"end":303,"count":10},
 {"start":213,"end":303,"count":0},
 {"start":510,"end":603,"count":0},
 {"start":811,"end":1000,"count":0}]
);

TestCoverage(
"do-while statement (early return)",
`
!function() {                             // 0000
  let i = 0;                              // 0050
  do {                                    // 0100
    i++;                                  // 0150
    continue;                             // 0200
    nop();                                // 0250
  } while (i < 10);                       // 0300
  nop();                                  // 0350
  do {                                    // 0400
    nop();                                // 0450
    break;                                // 0500
    nop();                                // 0550
  } while (true);                         // 0600
  nop();                                  // 0650
  do {                                    // 0700
    nop();                                // 0750
    return;                               // 0800
    nop();                                // 0850
  } while (true);                         // 0900
  nop();                                  // 0950
}()                                       // 1000
`,
[{"start":0,"end":1049,"count":1},
 {"start":1,"end":1001,"count":1},
 {"start":105,"end":303,"count":10},
 {"start":213,"end":303,"count":0},
 {"start":510,"end":603,"count":0},
 {"start":811,"end":1000,"count":0}]
);

TestCoverage(
"return statements",
`
!function() { nop(); return; nop(); }();  // 0000
!function() { nop(); return 42;           // 0050
              nop(); }();                 // 0100
`,
[{"start":0,"end":149,"count":1},
 {"start":1,"end":37,"count":1},
 {"start":28,"end":36,"count":0},
 {"start":51,"end":122,"count":1},
 {"start":81,"end":121,"count":0}]
);

TestCoverage(
"try/catch/finally statements",
`
!function() {                             // 0000
  try { nop(); } catch (e) { nop(); }     // 0050
  try { nop(); } finally { nop(); }       // 0100
  try {                                   // 0150
    try { throw 42; } catch (e) { nop(); }// 0200
  } catch (e) { nop(); }                  // 0250
  try {                                   // 0300
    try { throw 42; } finally { nop(); }  // 0350
  } catch (e) { nop(); }                  // 0400
  try {                                   // 0450
    throw 42;                             // 0500
  } catch (e) {                           // 0550
    nop();                                // 0600
  } finally {                             // 0650
    nop();                                // 0700
  }                                       // 0750
}();                                      // 0800
`,
[{"start":0,"end":849,"count":1},
 {"start":1,"end":801,"count":1},
 {"start":67,"end":87,"count":0},
 {"start":219,"end":222,"count":0},
 {"start":254,"end":274,"count":0},
 {"start":369,"end":372,"count":0},
 {"start":390,"end":404,"count":0},
 {"start":513,"end":554,"count":0}]
);

TestCoverage("try/catch/finally statements with early return",
`
!function() {                             // 0000
  try { throw 42; } catch (e) { return; } // 0050
  nop();                                  // 0100
}();                                      // 0150
!function() {                             // 0200
  try { throw 42; } catch (e) {}          // 0250
  finally { return; }                     // 0300
  nop();                                  // 0350
}();                                      // 0400
`,
[{"start":0,"end":449,"count":1},
 {"start":1,"end":151,"count":1},
 {"start":67,"end":70,"count":0},
 {"start":89,"end":150,"count":0},
 {"start":201,"end":401,"count":1},
 {"start":267,"end":270,"count":0},
 {"start":319,"end":400,"count":0}]
);

TestCoverage(
"early return in blocks",
`
!function() {                             // 0000
  try { throw 42; } catch (e) { return; } // 0050
  nop();                                  // 0100
}();                                      // 0150
!function() {                             // 0200
  try { nop(); } finally { return; }      // 0250
  nop();                                  // 0300
}();                                      // 0350
!function() {                             // 0400
  {                                       // 0450
    let x = 42;                           // 0500
    return () => x;                       // 0550
  }                                       // 0600
  nop();                                  // 0650
}();                                      // 0700
!function() {                             // 0750
  try { throw 42; } catch (e) {           // 0800
    return;                               // 0850
    nop();                                // 0900
  }                                       // 0950
  nop();                                  // 1000
}();                                      // 1050
`,
[{"start":0,"end":1099,"count":1},
 {"start":1,"end":151,"count":1},
 {"start":67,"end":70,"count":0},
 {"start":89,"end":150,"count":0},
 {"start":201,"end":351,"count":1},
 {"start":284,"end":350,"count":0},
 {"start":401,"end":701,"count":1},
 {"start":569,"end":700,"count":0},
 {"start":561,"end":568,"count":0},  // TODO(jgruber): Sorting.
 {"start":751,"end":1051,"count":1},
 {"start":817,"end":820,"count":0},
 {"start":861,"end":1050,"count":0}]
);

TestCoverage(
"switch statements",
`
!function() {                             // 0000
  var x = 42;                             // 0050
  switch (x) {                            // 0100
    case 41: nop(); break;                // 0150
    case 42: nop(); break;                // 0200
    default: nop(); break;                // 0250
  }                                       // 0300
}();                                      // 0350
`,
[{"start":0,"end":399,"count":1},
 {"start":1,"end":351,"count":1},
 {"start":154,"end":204,"count":0},
 {"start":226,"end":303,"count":0}]
);

TestCoverage(
"labeled break statements",
`
!function() {                             // 0000
  var x = 42;                             // 0050
  l0: switch (x) {                        // 0100
  case 41: return;                        // 0150
  case 42:                                // 0200
    switch (x) { case 42: break l0; }     // 0250
    break;                                // 0300
  }                                       // 0350
  l1: for (;;) {                          // 0400
    for (;;) break l1;                    // 0450
  }                                       // 0500
  l2: while (true) {                      // 0550
    while (true) break l2;                // 0600
  }                                       // 0650
  l3: do {                                // 0700
    do { break l3; } while (true);        // 0750
  } while (true);                         // 0800
  l4: { break l4; }                       // 0850
  l5: for (;;) for (;;) break l5;         // 0900
}();                                      // 0950
`,
[{"start":0,"end":999,"count":1},
 {"start":1,"end":951,"count":1},
 {"start":152,"end":202,"count":0},
 {"start":285,"end":353,"count":0},
 {"start":472,"end":503,"count":0},
 {"start":626,"end":653,"count":0},
 {"start":768,"end":803,"count":0},
 {"start":867,"end":869,"count":0}]
);

TestCoverage(
"labeled continue statements",
`
!function() {                             // 0000
  l0: for (var i0 = 0; i0 < 2; i0++) {    // 0050
    for (;;) continue l0;                 // 0100
  }                                       // 0150
  var i1 = 0;                             // 0200
  l1: while (i1 < 2) {                    // 0250
    i1++;                                 // 0300
    while (true) continue l1;             // 0350
  }                                       // 0400
  var i2 = 0;                             // 0450
  l2: do {                                // 0500
    i2++;                                 // 0550
    do { continue l2; } while (true);     // 0600
  } while (i2 < 2);                       // 0650
}();                                      // 0700
`,
[{"start":0,"end":749,"count":1},
 {"start":1,"end":701,"count":1},
 {"start":87,"end":153,"count":2},
 {"start":125,"end":153,"count":0},
 {"start":271,"end":403,"count":2},
 {"start":379,"end":403,"count":0},
 {"start":509,"end":653,"count":2},
 {"start":621,"end":653,"count":0}]
);

TestCoverage(
"conditional expressions",
`
var TRUE = true;                          // 0000
var FALSE = false;                        // 0050
!function() {                             // 0100
  TRUE ? nop() : nop();                   // 0150
  true ? nop() : nop();                   // 0200
  false ? nop() : nop();                  // 0250
  FALSE ? TRUE ? nop()                    // 0300
               : nop()                    // 0350
        : nop();                          // 0400
  TRUE ? FALSE ? nop()                    // 0450
               : nop()                    // 0500
       : nop();                           // 0550
  TRUE ? nop() : FALSE ? nop()            // 0600
                       : nop();           // 0650
  FALSE ? nop() : TRUE ? nop()            // 0700
                       : nop();           // 0750
}();                                      // 0800
`,
[{"start":0,"end":849,"count":1},
 {"start":101,"end":801,"count":1},
 {"start":165,"end":172,"count":0},
 {"start":215,"end":222,"count":0},
 {"start":258,"end":265,"count":0},
 {"start":308,"end":372,"count":0},
 {"start":465,"end":472,"count":0},
 {"start":557,"end":564,"count":0},
 {"start":615,"end":680,"count":0},
 {"start":708,"end":715,"count":0},
 {"start":773,"end":780,"count":0}]
);

TestCoverage(
"yield expressions",
`
const it = function*() {                  // 0000
  yield nop();                            // 0050
  yield nop() ? nop() : nop()             // 0100
  return nop();                           // 0150
}();                                      // 0200
it.next(); it.next();                     // 0250
`,
[{"start":0,"end":299,"count":1},
 {"start":11,"end":201,"count":3},
 {"start":64,"end":114,"count":1},
 {"start":114,"end":121,"count":0},
 {"start":122,"end":129,"count":1},
 {"start":129,"end":200,"count":0}]
);

TestCoverage(
"yield expressions (.return and .throw)",
`
const it0 = function*() {                 // 0000
  yield 1; yield 2; yield 3;              // 0050
}();                                      // 0100
it0.next(); it0.return();                 // 0150
try {                                     // 0200
  const it1 = function*() {               // 0250
    yield 1; yield 2; yield 3;            // 0300
  }();                                    // 0350
  it1.next(); it1.throw();                // 0400
} catch (e) {}                            // 0450
`,
[{"start":0,"end":499,"count":1},
 {"start":12,"end":101,"count":3},
 {"start":60,"end":100,"count":0},
 {"start":264,"end":353,"count":3},
 {"start":312,"end":352,"count":0}]
);

TestCoverage("yield expressions (.return and try/catch/finally)",
`
const it = function*() {                  // 0000
  try {                                   // 0050
    yield 1; yield 2; yield 3;            // 0100
  } catch (e) {                           // 0150
    nop();                                // 0200
  } finally { nop(); }                    // 0250
  yield 4;                                // 0300
}();                                      // 0350
it.next(); it.return();                   // 0450
`,
[{"start":0,"end":449,"count":1},
 {"start":11,"end":351,"count":3},
 {"start":112,"end":254,"count":0},
 {"start":254,"end":272,"count":1},
 {"start":272,"end":350,"count":0}]
);

TestCoverage("yield expressions (.throw and try/catch/finally)",
`
const it = function*() {                  // 0000
  try {                                   // 0050
    yield 1; yield 2; yield 3;            // 0100
  } catch (e) {                           // 0150
    nop();                                // 0200
  } finally { nop(); }                    // 0250
  yield 4;                                // 0300
}();                                      // 0350
it.next(); it.throw(42);                  // 0550
`,
[{"start":0,"end":449,"count":1},
 {"start":11,"end":351,"count":3},
 {"start":112,"end":154,"count":0},
 {"start":154,"end":310,"count":1},
 {"start":310,"end":350,"count":0}]
);

TestCoverage(
"yield* expressions",
`
const it = function*() {                  // 0000
  yield* gen();                           // 0050
  yield* nop() ? gen() : gen()            // 0100
  return gen();                           // 0150
}();                                      // 0200
it.next(); it.next(); it.next();          // 0250
it.next(); it.next(); it.next();          // 0300
`,
[{"start":0,"end":349,"count":1},
 {"start":11,"end":201,"count":7},
 {"start":65,"end":115,"count":1},
 {"start":115,"end":122,"count":0},
 {"start":123,"end":130,"count":1},
 {"start":130,"end":200,"count":0}]
);

TestCoverage(
"yield* expressions (.return and .throw)",
`
const it0 = function*() {                 // 0000
  yield* gen(); yield* gen(); yield 3;    // 0050
}();                                      // 0100
it0.next(); it0.return();                 // 0150
try {                                     // 0200
  const it1 = function*() {               // 0250
    yield* gen(); yield* gen(); yield 3;  // 0300
  }();                                    // 0350
  it1.next(); it1.throw();                // 0400
} catch (e) {}                            // 0450
`,
[{"start":0,"end":499,"count":1},
 {"start":12,"end":101,"count":3},
 {"start":65,"end":100,"count":0},
 {"start":264,"end":353,"count":3},
 {"start":317,"end":352,"count":0}]
);

TestCoverage(
"await expressions",
`
async function f() {                      // 0000
  await 42;                               // 0050
  await 42;                               // 0100
};                                        // 0150
f();                                      // 0200
%RunMicrotasks();                         // 0250
`,
[{"start":0,"end":299,"count":1},
 {"start":0,"end":151,"count":3},
 {"start":61,"end":150,"count":1}]
);

TestCoverage(
"LogicalOrExpression assignment",
`
const a = true || 99                      // 0000
function b () {                           // 0050
  const b = a || 2                        // 0100
}                                         // 0150
b()                                       // 0200
b()                                       // 0250
`,
[{"start":0,"end":299,"count":1},
 {"start":15,"end":20,"count":0},
 {"start":50,"end":151,"count":2},
 {"start":114,"end":118,"count":0}]);

TestCoverage(
"LogicalOrExpression IsTest()",
`
true || false                             // 0000
const a = 99                              // 0050
a || 50                                   // 0100
const b = false                           // 0150
if (b || true) {}                         // 0200
`,
[{"start":0,"end":249,"count":1},
 {"start":5,"end":13,"count":0},
 {"start":102,"end":107,"count":0}]);

TestCoverage(
"LogicalAndExpression assignment",
`
const a = false && 99                     // 0000
function b () {                           // 0050
  const b = a && 2                        // 0100
}                                         // 0150
b()                                       // 0200
b()                                       // 0250
const c = true && 50                      // 0300
`,
[{"start":0,"end":349,"count":1},
 {"start":16,"end":21,"count":0},
 {"start":50,"end":151,"count":2},
 {"start":114,"end":118,"count":0}]);

TestCoverage(
"LogicalAndExpression IsTest()",
`
false && true                             // 0000
const a = 0                               // 0050
a && 50                                   // 0100
const b = true                            // 0150
if (b && true) {}                         // 0200
true && true                              // 0250
`,
[{"start":0,"end":299,"count":1},
 {"start":6,"end":13,"count":0},
 {"start":102,"end":107,"count":0}]);

TestCoverage(
"NaryLogicalOr assignment",
`
const a = true                            // 0000
const b = false                           // 0050
const c = false || false || 99            // 0100
const d = false || true || 99             // 0150
const e = true || true || 99              // 0200
const f = b || b || 99                    // 0250
const g = b || a || 99                    // 0300
const h = a || a || 99                    // 0350
const i = a || (b || c) || d              // 0400
`,
[{"start":0,"end":449,"count":1},
 {"start":174,"end":179,"count":0},
 {"start":215,"end":222,"count":0},
 {"start":223,"end":228,"count":0},
 {"start":317,"end":322,"count":0},
 {"start":362,"end":366,"count":0},
 {"start":367,"end":372,"count":0},
 {"start":412,"end":423,"count":0},
 {"start":424,"end":428,"count":0}]);

TestCoverage(
"NaryLogicalOr IsTest()",
`
const a = true                            // 0000
const b = false                           // 0050
false || false || 99                      // 0100
false || true || 99                       // 0150
true || true || 99                        // 0200
b || b || 99                              // 0250
b || a || 99                              // 0300
a || a || 99                              // 0350
`,
[{"start":0,"end":399,"count":1},
 {"start":164,"end":169,"count":0},
 {"start":205,"end":212,"count":0},
 {"start":213,"end":218,"count":0},
 {"start":307,"end":312,"count":0},
 {"start":352,"end":356,"count":0},
 {"start":357,"end":362,"count":0}]);

TestCoverage(
"NaryLogicalAnd assignment",
`
const a = true                            // 0000
const b = false                           // 0050
const c = false && false && 99            // 0100
const d = false && true && 99             // 0150
const e = true && true && 99              // 0200
const f = true && false || true           // 0250
const g = true || false && true           // 0300
`,
[{"start":0,"end":349,"count":1},
 {"start":116,"end":124,"count":0},
 {"start":125,"end":130,"count":0},
 {"start":166,"end":173,"count":0},
 {"start":174,"end":179,"count":0},
 {"start":315,"end":331,"count":0}
]);

TestCoverage(
"NaryLogicalAnd IsTest()",
`
const a = true                            // 0000
const b = false                           // 0050
false && false && 99                      // 0100
false && true && 99                       // 0150
true && true && 99                        // 0200
true && false || true                     // 0250
true || false && true                     // 0300
false || false || 99 || 55                // 0350
`,
[{"start":0,"end":399,"count":1},
 {"start":106,"end":114,"count":0},
 {"start":115,"end":120,"count":0},
 {"start":156,"end":163,"count":0},
 {"start":164,"end":169,"count":0},
 {"start":305,"end":321,"count":0},
 {"start":371,"end":376,"count":0}]);

// see regression: https://bugs.chromium.org/p/chromium/issues/detail?id=785778
TestCoverage(
"logical expressions + conditional expressions",
`
const a = true                            // 0000
const b = 99                              // 0050
const c = false                           // 0100
const d = ''                              // 0150
const e = a && (b ? 'left' : 'right')     // 0200
const f = a || (b ? 'left' : 'right')     // 0250
const g = c || d ? 'left' : 'right'       // 0300
const h = a && b && (b ? 'left' : 'right')// 0350
const i = d || c || (c ? 'left' : 'right')// 0400
`,
[{"start":0,"end":449,"count":1},
 {"start":227,"end":236,"count":0},
 {"start":262,"end":287,"count":0},
 {"start":317,"end":325,"count":0},
 {"start":382,"end":391,"count":0},
 {"start":423,"end":431,"count":0}
]);

%DebugToggleBlockCoverage(false);
