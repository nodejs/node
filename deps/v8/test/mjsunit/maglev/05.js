// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(i, end) {
  do {
    do {
      i = end;
    } while (end);
    end = i;
  } while (i);
  return 10;
}

%PrepareFunctionForOptimization(f);
assertEquals(10, f(false, false));

%OptimizeMaglevOnNextCall(f);
assertEquals(10, f(false, false));
