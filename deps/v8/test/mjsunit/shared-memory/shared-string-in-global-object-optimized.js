// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --shared-string-table --harmony-struct


var ST = new SharedStructType(['foo']);
var t22 = new ST();

function f() {
  t22.foo = 'a'.repeat(9);
  globalThis[Symbol.unscopables] = t22.foo;
  return globalThis[Symbol.unscopables];
}

%PrepareFunctionForOptimization(f);
for (let i = 0; i < 10; i++) {
  f();
}
%OptimizeFunctionOnNextCall(f);
assertEquals('a'.repeat(9), f());
