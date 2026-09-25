// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var array = [[]];
function foo() {
  const x = array[0];
  const y = [][0];
  return x == y;
}
%PrepareFunctionForOptimization(foo);
assertFalse(foo());
%OptimizeFunctionOnNextCall(foo);
assertFalse(foo());
