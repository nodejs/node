// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function f(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11) {
  for (let i = 0; i < 0; i++) {}
  try {
    throw 42;
  } catch (e) {
  }
}

%PrepareFunctionForOptimization(f);
f(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42);
f(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42);
%OptimizeMaglevOnNextCall(f);
f(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42);
