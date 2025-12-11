// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

const obj = new class C extends async function () {}.constructor {}();
delete obj.name;

Number.prototype.__proto__ = obj;
function foo() {
  return obj.bind();
}

%PrepareFunctionForOptimization(foo);
foo();
obj[undefined] = Map, gc();
%OptimizeFunctionOnNextCall(foo);
foo();
