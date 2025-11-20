// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-turbo-store-elimination

function foo(val) {
    switch (val) {
      case 5:
      case 4:
        return "aha";
      case 5:
      case 6:
        return "na'ah";
    }
}

%PrepareFunctionForOptimization(foo);
assertEquals("aha", foo(5));
assertEquals("na'ah", foo(6));

%OptimizeFunctionOnNextCall(foo);
assertEquals("aha", foo(5));
