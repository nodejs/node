// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --maglev-inlining --allow-natives-syntax

function hasInstance(x) {
  %DeoptimizeFunction(bar);
  return 5;
}

function Foo() {}
Object.defineProperty(Foo, Symbol.hasInstance, {
  value: hasInstance
})

let foo = new Foo();

function bar(x) {
  return x instanceof Foo;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(hasInstance);
assertTrue(bar(foo));
assertTrue(bar(foo));

%OptimizeMaglevOnNextCall(bar);
assertTrue(bar(foo));
