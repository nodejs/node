// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0


const arr = [11, 22, 33];

function my_array() {
  return arr;
}

function inner() {
  for (let i = 0; i < 5; i++);
  return Array.isArray;
}

function my_call(f, arg) {
  return f(arg)
}

function foo() {
  return my_call(inner(), my_array());
}

%PrepareFunctionForOptimization(my_array);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(my_call);
%PrepareFunctionForOptimization(foo);
foo();
foo();

my_call(() => {}, {});

%OptimizeFunctionOnNextCall(foo);
foo();
assertOptimized(foo);
