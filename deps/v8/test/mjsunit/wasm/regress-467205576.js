// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(makeSig([kWasmExternRef, kWasmI32, kWasmF32], []));
let w0 = builder.addFunction(undefined, $sig0).exportAs('w0')
    .addLocals(kWasmI32, 1)  // $var3
    .addBody([
      kExprLocalGet, 0,  // $var0
      kExprRefIsNull,
      kExprLocalSet, 3,  // $var3
    ]);

const v6 = builder.instantiate();

const v7 = v6.exports;
function foo() {
  v7.w0();
}

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
