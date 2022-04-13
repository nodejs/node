// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const o = {};

function foo() {
  return o[4294967295];
};
%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo());
assertEquals(undefined, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
