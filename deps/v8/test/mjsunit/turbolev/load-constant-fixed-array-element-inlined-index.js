// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --maglev-object-tracking
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function get_const() {
  return 3;
}

function foo() {
  let arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let index = get_const();
  return arr[index];
}

%PrepareFunctionForOptimization(get_const);
%PrepareFunctionForOptimization(foo);
assertEquals(4, foo());
assertEquals(4, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(4, foo());
assertOptimized(foo);
