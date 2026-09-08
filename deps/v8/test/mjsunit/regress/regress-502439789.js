// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  const v2 = ['a', 'b', 'c'];
  for (let v3 = 0; v3 < 5; v3++) v2.sort(Array);
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
