// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing



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

// Transplant the dispatch handle from one function to another.
// This should simply result in the execution of different (but valid) code.
assertEquals(f1(), 42);
assertEquals(f2(), 43);
let dispatch_handle1 = Sandbox.readObjectField(f1, 'dispatch_handle');
let dispatch_handle2 = Sandbox.readObjectField(f2, 'dispatch_handle');
Sandbox.corruptObjectField(f2, 'dispatch_handle', dispatch_handle1);
Sandbox.corruptObjectField(f1, 'dispatch_handle', dispatch_handle2);
assertEquals(f1(), 43);
assertEquals(f2(), 42);
