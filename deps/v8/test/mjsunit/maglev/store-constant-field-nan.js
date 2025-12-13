// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Constant field with NaN value.
let x = NaN;

// Creating a NaN with non-canonical bit-pattern.
let dv = new DataView(new ArrayBuffer(8));
dv.setInt32(0, 0x7ff80000);
dv.setInt32(4, 0x000ff000);
let custom_nan = dv.getFloat64(0);

function foo() {
  x = custom_nan;
  x = custom_nan;
}

%PrepareFunctionForOptimization(foo);
// Not collecting feedback so that the runtime doesn't decide that the field
// isn't constant because of nan bit pattern changes.

%OptimizeMaglevOnNextCall(foo);
foo(NaN);
assertTrue(isMaglevved(foo));
assertEquals(x, NaN);
assertEquals(x, custom_nan);
