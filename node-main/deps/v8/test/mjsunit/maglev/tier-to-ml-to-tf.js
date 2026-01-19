// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --no-baseline-batch-compilation

function f(x) {
  var y = 0;
  for (var i = 0; i < x; i++) {
    y = 1;
  }
  return y;
}

let keep_going = 10000000;  // A counter to avoid test hangs on failure.

function g() {
  // Test that normal tiering (without OptimizeFooOnNextCall) works.
  // We test the entire pipeline, i.e. Ignition-SP-ML-TF.

  f(10);

  if (%IsSparkplugEnabled()) {
    while (!%ActiveTierIsSparkplug(f) && --keep_going) f(10);
    assertTrue(%ActiveTierIsSparkplug(f));
  }

  if (%IsMaglevEnabled()) {
    while (!%ActiveTierIsMaglev(f) && --keep_going) f(10);
    assertTrue(%ActiveTierIsMaglev(f));
  }

  if (%IsTurbofanEnabled()) {
    while (!%ActiveTierIsTurbofan(f) && --keep_going) f(10);
    assertTrue(%ActiveTierIsTurbofan(f));

    f(10);
    assertTrue(%ActiveTierIsTurbofan(f));
  }
}
%NeverOptimizeFunction(g);

g();
