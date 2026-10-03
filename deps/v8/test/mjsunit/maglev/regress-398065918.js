// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function __f_0() {
  let x = 2.2;
  let y = {a: {}, b: [x], c: {}};
  console.log("");
}

%PrepareFunctionForOptimization(__f_0);
__f_0();
__f_0();
%OptimizeMaglevOnNextCall(__f_0);
__f_0();
