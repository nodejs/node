// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(b, n) {
  let a;
  if (b) {
    a = 2;
  } else {
    a = n%n;
  }
  return a;
}

%PrepareFunctionForOptimization(f);
f(false, 2);
%OptimizeMaglevOnNextCall(f);
f(false, 2);
