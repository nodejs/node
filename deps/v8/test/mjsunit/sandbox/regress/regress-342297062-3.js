// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --no-maglev-inlining --no-turbo-inlining



let params = [];
for (let i = 1; i <= 10000; i++) {
  params.push(`a${i}`);
}
let body = 'return 42;';
let f1 = new Function(...params, body);
%PrepareFunctionForOptimization(f1);
f1();
%OptimizeFunctionOnNextCall(f1);
f1();

let f2 = new Function('a', 'b', 'c', 'return 43;');
%PrepareFunctionForOptimization(f2);
f2();
%OptimizeFunctionOnNextCall(f2);
f2();

// Inlining needs to be disabled here so the JIT compiler emits calls to the
// (known) functions instead of inlining them.
function caller1() {
  f1();
}
function caller2() {
  f2();
}
%PrepareFunctionForOptimization(caller1);
%PrepareFunctionForOptimization(caller2);
caller1();
caller2();
%OptimizeFunctionOnNextCall(caller1);
%OptimizeFunctionOnNextCall(caller2);
caller1();
caller2();

// Swap the dispatch handles of the two functions.
let dispatch_handle1 = Sandbox.readObjectField(f1, 'dispatch_handle');
let dispatch_handle2 = Sandbox.readObjectField(f2, 'dispatch_handle');
Sandbox.corruptObjectField(f2, 'dispatch_handle', dispatch_handle1);
Sandbox.corruptObjectField(f1, 'dispatch_handle', dispatch_handle2);

caller1();
caller2();
// Should either crash safely or succeed.
