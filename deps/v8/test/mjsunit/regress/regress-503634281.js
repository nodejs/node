// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Regression test for crbug.com/503634281. Segfault in MaglevGraphBuilder
// (AttachExceptionHandlerInfo) when compiling a JS-API builtin call
// (Worker.postMessage) inside a peeled loop.

function f1() { return ""; }
const worker = new Worker(f1, { type: "function" });
const obj = { __proto__: worker };

function test() {
  for (let i = 0; i < 5; i++) {
    const proto = obj.__proto__;
    let v;
    try { v = proto.postMessage(); } catch (e) {}
    proto[0] = "x";
    function getter() { return v; }
    function setter(a) { return setter; }
    Object.defineProperty(proto, "length", { configurable: true, get: getter, set: setter });
    %OptimizeOsr();
  }
}
%PrepareFunctionForOptimization(test);
test();
