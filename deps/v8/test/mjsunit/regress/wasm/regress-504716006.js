// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function test(numLocals) {
    let builder = new WasmModuleBuilder();
    let innerIdx = builder.addStruct([], true);
    let outerIdx = builder.addStruct([makeField(wasmRefNullType(innerIdx), true)], true);

    builder.addFunction("createInner", makeSig([], [wasmRefType(innerIdx)]))
      .addBody([kGCPrefix, kExprStructNew, innerIdx])
      .exportAs("createInner");

    builder.addFunction("createOuter", makeSig([wasmRefType(innerIdx)], [wasmRefType(outerIdx)]))
      .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, outerIdx])
      .exportAs("createOuter");

    let args = [wasmRefType(outerIdx), wasmRefNullType(innerIdx), wasmRefNullType(innerIdx)];
    for (let i = 0; i < numLocals; i++) args.push(kWasmI32);

    let body = [];
    for (let i = 0; i < numLocals; i++) body.push(kExprLocalGet, 3 + i);
    body.push(kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2);
    body.push(kAtomicPrefix, kExprStructAtomicCompareExchange, 0, outerIdx, 0);
    // Stack is [i32..., result]
    let resultLocal = 3 + numLocals;
    body.push(kExprLocalSet, resultLocal);
    for (let i = 0; i < numLocals; i++) body.push(kExprDrop);
    body.push(kExprLocalGet, resultLocal);

    builder.addFunction("main", makeSig(args, [wasmRefNullType(innerIdx)]))
      .addLocals(wasmRefNullType(innerIdx), 1)
      .addBody(body)
      .exportAs("main");

    let instance = builder.instantiate();
    let inner1 = instance.exports.createInner();
    let inner2 = instance.exports.createInner();
    let outer = instance.exports.createOuter(inner1);

    let res = instance.exports.main(outer, inner1, inner2, ...new Array(numLocals).fill(0));
    assertEquals(res, inner1);
}

for (let i = 0; i < 15; i++) {
    print(`Testing with ${i} locals...`);
    test(i);
}
