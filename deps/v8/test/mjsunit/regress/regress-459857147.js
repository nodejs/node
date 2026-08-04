// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f(__v_19) {
  Array(0, __v_19 >> __v_19);
}
%PrepareFunctionForOptimization(f);
f(undefined);
%OptimizeFunctionOnNextCall(f);
f(undefined);
