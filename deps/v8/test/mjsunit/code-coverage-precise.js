// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt

// Test precise code coverage.

function GetCoverage(source) {
  for (var script of %DebugCollectCoverage()) {
    if (script.script.source == source) return script;
  }
  return undefined;
}

function TestCoverage(name, source, expectation) {
  source = source.trim();
  eval(source);
  %CollectGarbage("collect dead objects");
  var coverage = GetCoverage(source);
  var result = JSON.stringify(coverage);
  print(result);
  assertEquals(JSON.stringify(expectation), result, name + " failed");
}

// Without precise coverage enabled, we lose coverage data to the GC.
TestCoverage(
"call an IIFE",
`
(function f() {})();
`,
undefined  // The IIFE has been garbage-collected.
);

TestCoverage(
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

TestCoverage(
"call an IIFE",
`
(function f() {})();
`,
[{"start":0,"end":20,"count":1},{"start":1,"end":16,"count":1}]
);

TestCoverage(
"call locally allocated function",
`
for (var i = 0; i < 10; i++) {
  let f = () => 1;
  i += f();
}
`,
[{"start":0,"end":63,"count":1},{"start":41,"end":48,"count":5}]
);

%DebugTogglePreciseCoverage(false);
