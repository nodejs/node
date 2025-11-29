// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --max-optimized-bytecode-size=300000

const args = new Array(35000).fill('arg');

// Regression test for ReduceJSCreateAsyncFunctionObject.
function outer_async() {
  async function g(replace_me) {}
  %PrepareFunctionForOptimization(g);
  %OptimizeFunctionOnNextCall(g);
  new Promise(g);
}

const outer_async_many_args = outer_async.toLocaleString().replace('replace_me', args);
eval(outer_async_many_args);
outer_async();

// Regression test for ReduceJSCreateBoundFunction.
function outer_bind(arg) {
  function b() { return 42; };
  return b.bind(null, replace_me);
}

const outer_bind_many_args = outer_bind.toLocaleString().replace('replace_me', args);
eval(outer_bind_many_args);
%PrepareFunctionForOptimization(outer_bind);
outer_bind();
%OptimizeFunctionOnNextCall(outer_bind);
outer_bind();
