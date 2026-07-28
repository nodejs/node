// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --verify-bytecode-full

function foo() {
  return 42;
}

%PrepareFunctionForOptimization(foo);
let bc = %GetBytecode(foo);
bc.frame_size = -56;
%InstallBytecode(foo, bc);

%OptimizeMaglevOnNextCall(foo);
foo();
