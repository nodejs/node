// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f(__v_51, __v_52, __v_53) {
  var a = false;
  var b = a;
  try {
    var c = false + false;
  } catch {}
  try {
    var d = false - (null == true);
  } catch {}
  return a + b - c + d;
}
%PrepareFunctionForOptimization(f);
assertEquals(0, f());
%OptimizeMaglevOnNextCall(f);
assertEquals(0, f());
