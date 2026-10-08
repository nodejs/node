// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function getConst() {
  return 42;
}

function caller() {
  let x = getConst();
  let y = x | 0;
  %TurbofanStaticAssert(y == 42);
}

%PrepareFunctionForOptimization(getConst);
%PrepareFunctionForOptimization(caller);
caller();
%OptimizeFunctionOnNextCall(caller);
caller();
