// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev-future

const dummy_obj = {};
const global_obj = { a: dummy_obj };

// We need the second parameter 'unused' to trigger a V8 arity mismatch
// inside the loop body.
function bar(o, unused) {
  o.b = dummy_obj;
  const local_obj = { "!": o };
  local_obj.b = local_obj;
}

function foo(a, b, c) {
  for (let i = 0; i < 100; i++) {
    bar(global_obj);
  }

  // We only need a store to global_obj.b after the loop to make o.b
  // start as Unobservable at the loop exit during the backward pass.
  global_obj.b = dummy_obj;

  return b;
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
