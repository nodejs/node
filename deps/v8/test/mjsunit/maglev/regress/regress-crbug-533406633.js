// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// A phi untagged to HoleyFloat64 that carries a SafeInt-truncation use (from
// the truncation pass for AdditiveSafeInteger arithmetic) must be handled by
// phi untagging via TruncateHoleyFloat64AsSafeIntToInt32 rather than DCHECK.
function foo() {
  let ret = 0;
  for (let i = 0; i < 5; i++) {
    %OptimizeOsr();
    // The hole from the HOLEY_DOUBLE_ELEMENTS literal forces the phi to
    // HoleyFloat64; `?? c` supplies its non-hole double input.
    const phi = ([, , , -1.975703042211233])[2] ?? -4294967295;
    // `phi ^ 5` is a truncated-int32 use feeding the truncation pass; the
    // `phi + 1` Float64 use is dead, so the phi still untags to HoleyFloat64.
    const bits = phi ^ 5;
    const dead = phi + 1;
    ret = bits;
  }
  return ret;
}

%PrepareFunctionForOptimization(foo);
// -4294967295 truncates to int32 1, and 1 ^ 5 == 4.
assertEquals(4, foo());
