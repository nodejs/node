// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0() {
  return Array(([f0,f0,f0])[1]).indexOf();
}

%PrepareFunctionForOptimization(f0);
f0();
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
