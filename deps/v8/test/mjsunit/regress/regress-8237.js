// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt
// Files: test/mjsunit/code-coverage-utils.js

%DebugToggleBlockCoverage(true);

TestCoverage(
"Repro for the bug",
`
function lib (n) {                        // 0000
  if (n >= 0) {                           // 0050
    if (n < 0) {                          // 0100
      return;                             // 0150
    }                                     // 0200
  } else if (foo()) {                     // 0250
  }                                       // 0300
}                                         // 0350
function foo () {                         // 0400
  console.log('foo')                      // 0450
  return false                            // 0500
}                                         // 0550
lib(1)                                    // 0600
`,
[{"start":0,"end":649,"count":1},
{"start":0,"end":351,"count":1},
{"start":115,"end":205,"count":0},
{"start":253,"end":303,"count":0},
{"start":400,"end":551,"count":0}]
);

TestCoverage(
"Variant with omitted brackets",
`
function lib (n) {                        // 0000
  if (n >= 0) {                           // 0050
    if (n < 0)                            // 0100
      return;                             // 0150
  }                                       // 0200
  else if (foo());                        // 0250
}                                         // 0300
function foo () {                         // 0350
  console.log('foo')                      // 0400
  return false                            // 0450
}                                         // 0500
lib(1)                                    // 0550
`,
[{"start":0,"end":599,"count":1},
{"start":0,"end":301,"count":1},
{"start":156,"end":163,"count":0},
{"start":203,"end":268,"count":0},
{"start":350,"end":501,"count":0}]
);

%DebugToggleBlockCoverage(false);
