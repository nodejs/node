// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-stress-opt

function f(x) {
  var y = 0;
  for (var i = 0; i < x; i++) {
    y = 1;
  }
  return y;
}

function g() {
  // Test that normal tiering (without OptimizeMaglevOnNextCall) works.
  for (let i = 0; i < 1000; i++) {
    if (%ActiveTierIsMaglev(f)) break;
    f(10);
  }
}
%NeverOptimizeFunction(g);

g();

assertTrue(%ActiveTierIsMaglev(f));
