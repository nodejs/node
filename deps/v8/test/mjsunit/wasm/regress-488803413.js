// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let emit_leb = (v) => {
    let res = [];
    do {
        let byte = v & 0x7f;
        v >>= 7;
        if (v !== 0) byte |= 0x80;
        res.push(byte);
    } while (v !== 0);
    return res;
};

let builder = new WasmModuleBuilder();
let structType = builder.addStruct([makeField(kWasmI32, true)]);
builder.addGlobal(wasmRefNullType(structType), true, false).exportAs("g0");
builder.addGlobal(wasmRefNullType(structType), true, false).exportAs("g1");

let body = [];

body.push(kExprGlobalGet, 0, kExprLocalSet, 1);

for (let i = 0; i < 100; i++) {
    body.push(kExprLocalGet, 1, kGCPrefix, kExprStructGet, structType, 0, kExprDrop);
}

body.push(kExprGlobalGet, 1, kExprLocalSet, 2);

body.push(kExprBlock, kWasmRefNull, structType);

for (let i = 0; i < 256; i++) {
    body.push(kExprBlock, kWasmVoid);
}

body.push(kExprLocalGet, 0, kExprBrTable);
body.push(...emit_leb(255));
for (let i = 0; i < 255; i++) {
    body.push(...emit_leb(i + 1));
}
body.push(...emit_leb(0));

for (let i = 255; i >= 0; i--) {
    body.push(kExprEnd);
    if (i === 0) {
        body.push(kExprLocalGet, 2, kExprBr, ...emit_leb(i));
    } else {
        body.push(kExprLocalGet, 1, kExprBr, ...emit_leb(i));
    }
}

body.push(kExprEnd, kGCPrefix, kExprStructGet, structType, 0);

builder.addFunction("trigger", makeSig([kWasmI32], [kWasmI32]))
  .addLocals(wasmRefNullType(structType), 3)
  .addBody(body).exportFunc();

builder.addFunction("create", makeSig([], []))
  .addBody([
    kExprI32Const, 42,
    kGCPrefix, kExprStructNew, structType,
    kExprGlobalSet, 0,
    kExprI32Const, 43,
    kGCPrefix, kExprStructNew, structType,
    kExprGlobalSet, 1
  ]).exportFunc();

let instance = builder.instantiate({});
instance.exports.create();

for (let i = 0; i <= 255; i++) {
  try { instance.exports.trigger(i); } catch(e) {}
}
