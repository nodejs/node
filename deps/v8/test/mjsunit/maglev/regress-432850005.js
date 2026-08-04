// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size=460

let kWasmI32 = 0x7f;

Object.defineProperty(globalThis, 'kExprI32Const', {});

function checkExpr(expr) {
  for (let b of expr) {
  }
}

class WasmModuleBuilder {
  addGlobal(init) {
    checkExpr(init);
  }
}

function wasmI32Const() {
  return [kExprI32Const, ...[]];
};

function foo(w) {
  w.addGlobal(wasmI32Const());
}

let w = new WasmModuleBuilder();

%PrepareFunctionForOptimization(checkExpr);
%PrepareFunctionForOptimization(wasmI32Const);
%PrepareFunctionForOptimization(w.addGlobal);
%PrepareFunctionForOptimization(foo);
foo(w);
foo(w);
%OptimizeMaglevOnNextCall(foo);
foo(w);
