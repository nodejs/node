// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(init, b, n, early_exit) {
  let loop_phi = init;
  for (let i = 0; i < n; i++) {
    loop_phi += 3.5;
  }
  if (early_exit) {
    // During feedback collection, we'll exit early here if {loop_phi} isn't a
    // Smi (so that we have Smi feedback later on).
    return;
  }

  // Making sure that {loop_phi} is a known smi.
  loop_phi += 0;

  let phi = b ? loop_phi : 42;
  // {phi} is a known Smi and so we should have an UnsafeSmiUntag below (rather
  // than a CheckedSmiUntag).
  phi += 2;
  return phi;
}

%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo(5, false, 4, true));
assertEquals(44, foo(5, false, 0, false));
assertEquals(undefined, foo(2000000000, false, 0, true));

%OptimizeFunctionOnNextCall(foo);
assertEquals(44, foo(5, false, 4, false));

// Now we make sure that {loop_phi} is out of Smi range (but still Int32).
assertEquals(2000000002, foo(2000000000, true, 0, false));
