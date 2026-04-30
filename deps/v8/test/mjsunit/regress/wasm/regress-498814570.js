// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --experimental-wasm-js-interop
// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.startRecGroup();
let $desc0 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({descriptor: $desc0});
/* $desc0 */ builder.addStruct({fields: [makeField(kWasmExternRef, false)], describes: $struct0});
builder.endRecGroup();

builder.addFunction("make", makeSig([kWasmExternRef], [kWasmAnyRef]))
  .exportFunc()
  .addBody([kExprLocalGet, 0,
            kGCPrefix, kExprStructNew, $desc0,
            kGCPrefix, kExprStructNewDefaultDesc, $struct0,
            ]);

let instance = builder.instantiate();

let proto1 = { "0": 42 };

let wasm_obj1 = instance.exports.make(proto1);
let wasm_obj2 = instance.exports.make(wasm_obj1);

function foo(obj) {
  return obj[0];
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(wasm_obj2));
assertEquals(42, foo(wasm_obj2));
%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(wasm_obj2));
