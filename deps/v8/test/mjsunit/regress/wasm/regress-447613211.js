// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.startRecGroup();
let $desc0 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({ descriptor: $desc0 });
/* $desc0 */ builder.addStruct({
  describes: $struct0,
  fields: [makeField(kWasmExternRef, false)],
});
builder.endRecGroup();

builder.addFunction("make", makeSig([kWasmExternRef], [kWasmAnyRef]))
  .exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprStructNew, $desc0,
    kGCPrefix, kExprStructNewDesc, $struct0,
  ]);

let instance = builder.instantiate();

let proto = {};
proto.inobj0 = 1;
proto.inobj1 = 1;
proto.inobj2 = 1;
proto.inobj3 = 1;
proto.c = 1;

let wasm_obj = instance.exports.make(proto);
wasm_obj.a;   // Make prototypes fast.
proto.c = 2;  // Defeat constant tracking.

let obj = {__proto__: wasm_obj};

function foo(x) {
  return x.c;
}

for (let i = 0; i < 20; i++) {
  foo(obj);
}

assertEquals(2, foo(obj));
delete proto.c;
assertEquals(undefined, foo(obj));
obj.__proto__ = {};
assertEquals(undefined, foo(obj));
