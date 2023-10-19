// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function f(x) {
  let c = x | -6;
  switch (c) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case -5: return 3;
  }
  return 0;
}

%PrepareFunctionForOptimization(f);
assertEquals(0, f(-2147483648));
assertEquals(3, f(-2127484783));
%OptimizeMaglevOnNextCall(f);
assertEquals(0, f(-2147483648));
assertEquals(3, f(-2127484783));
