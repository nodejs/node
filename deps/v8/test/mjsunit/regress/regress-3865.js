// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {
  var radix = 10;
  return 21 / radix | 0;
}
assertEquals(2, bar());
assertEquals(2, bar());
%OptimizeFunctionOnNextCall(bar);
assertEquals(2, bar());
