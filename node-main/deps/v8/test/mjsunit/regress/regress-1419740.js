// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0() {
  for (let v5 = 1; v5 < 9; v5++) {
      function f6() {
      }
      function f10(a11) {
          for (let v15 = -1834309537; v15 < 9; v15 = v15 + a11) {
              function f17() {
              }
              for (let v20 = 0; v20 < v15; v20 = v20 + "14") {
              }
          }
          return f10;
      }
      const v21 = f10();
      v21(f6);
      v21();
  }
}
%PrepareFunctionForOptimization(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
