// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

function cb() {
  return arguments.length;
}
%NeverOptimizeFunction(cb);

// Passing `kArgCount` distinct expressions to a non-inlined call keeps all of
// them live simultaneously at the call site, requiring > 1024 stack slots
// (> 4KB on both 32-bit and 64-bit targets) while keeping the bytecode small
// enough to be compiled.
const kArgCount = 1100;
const args = Array.from({length: kArgCount}, (_, k) => `p ^ ${k + 1}`);

function buildWideFrame(osr) {
  return Function('x', 'cb', 'p', `
    let result = 0;
    for (let i = 0; i < 10; ++i) {
      ${osr ? 'if (i === 5) %OptimizeOsr();' : ''}
      if (x < 0) return cb(${args});
      result = (x + i) | 0;
    }
    return result;
  `);
}

const wide = buildWideFrame(false);
%PrepareFunctionForOptimization(wide);
assertEquals(kArgCount, wide(-1, cb, 1));
assertEquals(10, wide(1, cb, 1));
%OptimizeFunctionOnNextCall(wide);
assertEquals(10, wide(1, cb, 1));

const wideOsr = buildWideFrame(true);
%PrepareFunctionForOptimization(wideOsr);
assertEquals(kArgCount, wideOsr(-1, cb, 1));
assertEquals(10, wideOsr(1, cb, 1));
