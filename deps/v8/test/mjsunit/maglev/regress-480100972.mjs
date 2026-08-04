// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

export function f() {
    return y;
}

%PrepareFunctionForOptimization(f);
%OptimizeMaglevOnNextCall(f);

try {
  const v = f();
  new Map().delete(v);
} catch {}

export let y;
