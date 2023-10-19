// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

function foo() {
  try {

  } finally {
    return 1;
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals(foo(), 1);
assertEquals(foo(), 1);
%OptimizeMaglevOnNextCall(foo);
assertEquals(foo(), 1)
// Maglev will not compile this function now because it has handler table.
// After exceptions are supported in Maglev, we could enable the assert below.
// assertTrue(isMaglevved(foo));
