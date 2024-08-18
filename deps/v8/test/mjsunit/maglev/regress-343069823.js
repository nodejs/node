// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo() {
  const a = [1];
  a[3296295157] = 2;
  let [x] = a;
  return x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo());
assertEquals(1, foo());

%OptimizeMaglevOnNextCall(foo);
assertEquals(1, foo());

%OptimizeMaglevOnNextCall(foo);
assertEquals(1, foo());
// Check we don't have a deopt loop, ie, we don't try to optimize
// ArrayProtoNext again.
assertTrue(isOptimized(foo));
