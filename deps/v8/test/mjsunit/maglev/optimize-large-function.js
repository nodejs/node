// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

// Functions above --max-optimized-bytecode-size (TurboFan's limit) but below
// --max-maglev-optimized-bytecode-size must still be optimizable by Maglev
// (e.g. the ~64KB zlib core function in JetStream2's octane-zlib).
function buildLongBytecode(count) {
  let body = '';
  for (let i = 0; i < count; ++i) body += `sum += ${i};`;
  return Function(`return function long(sum) { ${body} return sum; };`)();
}

const long = buildLongBytecode(12000);
%PrepareFunctionForOptimization(long);
const expected = long(0);
%OptimizeMaglevOnNextCall(long);
assertEquals(expected, long(0));
assertOptimized(long);
