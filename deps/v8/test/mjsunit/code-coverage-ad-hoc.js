// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt

// Test code coverage without explicitly activating it upfront.

function GetCoverage(source) {
  for (var script of %DebugCollectCoverage()) {
    if (script.script.source == source) return script;
  }
  return undefined;
}

function TestCoverage(name, source, expectation) {
  source = source.trim();
  eval(source);
  var coverage = GetCoverage(source);
  var result = JSON.stringify(coverage);
  print(result);
  assertEquals(JSON.stringify(expectation), result, name + " failed");
}

TestCoverage(
"call simple function twice",
`
function f() {}
f();
f();
`,
[{"start":0,"end":25,"count":1},
 {"start":0,"end":15,"count":1}]
);

TestCoverage(
"call arrow function twice",
`
var f = () => 1;
f();
f();
`,
[{"start":0,"end":26,"count":1},
 {"start":8,"end":15,"count":1}]
);

TestCoverage(
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

TestCoverage(
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
