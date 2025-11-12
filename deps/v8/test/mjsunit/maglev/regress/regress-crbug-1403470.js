// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function __f_0( __v_18) {
  -1 % 11;
  return -1 % 11;
}
%PrepareFunctionForOptimization(__f_0);
assertEquals(-1, __f_0());
%OptimizeMaglevOnNextCall(__f_0);
assertEquals(-1, __f_0());
