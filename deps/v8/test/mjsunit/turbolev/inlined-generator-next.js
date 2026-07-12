// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

function* MyGen() {
  yield 45;
  yield 41;
  yield 18;
}

function foo() {
  let gen = MyGen();
  return gen.next().value + gen.next().value + gen.next().value;
}

%PrepareFunctionForOptimization(MyGen);
%PrepareFunctionForOptimization(foo);
assertEquals(104, foo());
assertEquals(104, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(104, foo());
