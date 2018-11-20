// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(string, radix) {
  // Use a phi to force radix into heap number representation.
  radix = (radix == 0) ? radix : (radix >> 0);
  if (radix != 2) return NaN;
  return %StringParseInt(string, radix);
}

assertEquals(2, (-4294967294) >> 0);
assertEquals(3, f("11", -4294967294));
assertEquals(NaN, f("11", -2147483650));
%OptimizeFunctionOnNextCall(f);
assertEquals(3, f("11", -4294967294));
