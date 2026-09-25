// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var bar = true;
bar = false;
function foo() {
  return !bar;
};
%PrepareFunctionForOptimization(foo);
assertEquals(foo(), true);
%OptimizeFunctionOnNextCall(foo);
assertEquals(foo(), true);
