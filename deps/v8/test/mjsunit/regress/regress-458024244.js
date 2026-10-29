// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0

function f6() {}
%PrepareFunctionForOptimization(f6);

function f0() {
  const v8 = Array(f6());
  try { v8.map(); } catch (e) {}
}
%PrepareFunctionForOptimization(f0);
f0();
%OptimizeMaglevOnNextCall(f0);
f0();
