// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-inline-js-wasm-wrappers --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

builder.addFunction("fn2", kSig_v_i)
  .addBody([
    ...wasmI32Const(0),
    kExprLocalGet, 0,
    kExprI32Add,
    kExprDrop,
  ])
  .exportAs("fn2");

let instance = builder.instantiate({});
let fn2 = instance.exports.fn2;

function fn0() {
  function fn1() {
    return fn2;
  }
  const t = fn1();
  const v = t();
  let r = v && v;
  assertEquals(undefined, r);
}

%PrepareFunctionForOptimization(fn0);
fn0();
%OptimizeFunctionOnNextCall(fn0);
fn0();
