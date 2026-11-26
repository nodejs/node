// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev --no-concurrent-osr

// Referencing all variables in dead code forces each of them into its own
// interpreter register while keeping the bytecode small. OSR then sees a live
// value in a register whose stack slot index exceeds the supported range.
const names = Array.from({length: 32800}, (_, i) => 'v' + i);
const f = new Function(`
  var ${names.join(',')};
  if (false) { ${names.join(';')} }
  v32799 = 42;
  var s = 0;
  for (var i = 0; i < 2; i++) {
    if (i == 0) %OptimizeOsr();
    assertFalse(topFrameIsTurboFanned(%GetOptimizationStatus(f)));
    s += v32799;
  }
  return s;
`);
%PrepareFunctionForOptimization(f);

assertEquals(84, f());
