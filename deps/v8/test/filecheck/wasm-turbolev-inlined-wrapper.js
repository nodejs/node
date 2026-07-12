// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --turbolev --turbolev-future --allow-natives-syntax
// Flags: --trace-turbo-filter=jsFunc --trace-turbo-graph --no-stress-maglev

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let sigReturnI32 = makeSig([], [kWasmI32]);
let builder = new WasmModuleBuilder();
let global = builder.addGlobal(kWasmI32, true, false).exportAs("val");
builder.addFunction("sub1", sigReturnI32)
.addLocals(kWasmI32, 1)
.addBody([
  kExprGlobalGet, global.index,
  kExprI32Const, 1,
  kExprI32Sub,
  kExprLocalTee, 0,
  kExprGlobalSet, global.index,
  kExprLocalGet, 0,
]).exportFunc();
let m = builder.instantiate().exports;

builder = new WasmModuleBuilder();
let callee = builder.addImport('m', 'sub1', sigReturnI32);
builder.addFunction("dec", sigReturnI32)
.addBody([kExprCallFunction, callee]).exportFunc();
let {dec} = builder.instantiate({m}).exports;

%PrepareFunctionForOptimization(jsFunc);
m.val.value = 1000;
jsFunc();
assertEquals(0, m.val.value);

%OptimizeFunctionOnNextCall(jsFunc);
m.val.value = 1000;
jsFunc();
assertEquals(0, m.val.value);

// Expectation: In the TurboshaftOptimize phase there is a call to a wasm
// function whose output directly goes into the branch instruction needed for
// the loop. There is no other usage of the DidntThrow output of the wasm call.

// CHECK-LABEL: ----- V8.TFTurboshaftOptimize -----
// CHECK: LOOP B{{[0-9]+}}
// CHECK: [[Call:[0-9]+]]: Call{{.*}}WasmFunctionIndirect
// CHECK: [[DidntThrow:[0-9]+]]: DidntThrow(#[[Call]])
// CHECK-NEXT: Branch(#[[DidntThrow]])
// CHECK-NOT: #[[DidntThrow]]
// CHECK-LABEL: -----
function jsFunc() {
  while (dec()) {  }
}
