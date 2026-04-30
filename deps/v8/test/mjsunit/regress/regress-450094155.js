// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f2() {
  while (true){
    for (let i = 0; i < 5; i++) {
      %OptimizeOsr();
      eval();
    }
    break;
  }
  return 0;
}
%PrepareFunctionForOptimization(f2);

f2();
f2();
