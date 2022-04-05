// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(i) {
  a:{
    b: {
      c: {
        if (i < 100) {
          break c;
        } else {
          break b;
        }
        i = 3;
      }
      i = 4;
      break a;
    }
    i = 5;
  }
  return i;
}


%PrepareFunctionForOptimization(f);
assertEquals(f(1), 4);

%OptimizeMaglevOnNextCall(f);
assertEquals(f(1), 4);
