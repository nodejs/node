// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function func() {
  function foo() {}
  return foo;
}

function bar(foo) {
  %DisassembleFunction(foo);
  foo();
  %PrepareFunctionForOptimization(foo);
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
}

bar(func());
bar(func());
bar(func());
