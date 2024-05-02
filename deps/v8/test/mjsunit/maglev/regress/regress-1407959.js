// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function func() {}
function foo(x) {
  return x instanceof func;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
foo();

let custom_has_instance_runs = 0;
Object.defineProperty(func, Symbol.hasInstance, {
  value: function() {
    custom_has_instance_runs++;
    return true;
  }
});

assertTrue(foo());
assertEquals(custom_has_instance_runs, 1);
