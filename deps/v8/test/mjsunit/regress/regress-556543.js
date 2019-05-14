// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  for (var __v_2 = 0; __v_2 < __v_5; ++__v_2) {
    for (var __v_5 = 0; __v_3 < 1; ++__v_8) {
      if (true || 0 > -6) continue;
      for (var __v_3 = 0; __v_3 < 1; ++__v_3) {
      }
    }
  }
}
%OptimizeFunctionOnNextCall(f);
f();
