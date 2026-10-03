// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0


const arr = [11, 22, 33];

function my_at() {
  for (let i = 0; i < 5; i++);
  return Array.prototype.at;
}

function a_index() {
  return 1;
}

function foo(a) {
  a[0];
  return my_at().call(a, a_index());
}

%PrepareFunctionForOptimization(my_at);
%PrepareFunctionForOptimization(a_index);
%PrepareFunctionForOptimization(foo);
assertEquals(22, foo(arr));
assertEquals(22, foo(arr));

%OptimizeFunctionOnNextCall(foo);
assertEquals(22, foo(arr));
assertOptimized(foo);
