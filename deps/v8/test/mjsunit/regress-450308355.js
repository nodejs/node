// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let captured;

function J_C() {
    let obj = {};
    Error.captureStackTrace(obj);
    captured = obj.stack;
}

function AsmModule(stdlib, foreign, heap) {
    "use asm";
    var callback = foreign.callback;
    function W() {
        callback();
    }
    return { W: W };
}

let exports = AsmModule(this, { callback: J_C }, new ArrayBuffer(65536));
let W = exports.W;

function B() {
  new W();
}

%PrepareFunctionForOptimization(B);
B();
// Stack minus last call.
let from_ignition = captured.replace(/\n.*$/, '');
%OptimizeMaglevOnNextCall(B);
B();
// Stack minus last call.
let from_optimized = captured.replace(/\n.*$/, '');

// Ignition and optimized code agrees with the stack trace.
assertEquals(from_ignition, from_optimized);
