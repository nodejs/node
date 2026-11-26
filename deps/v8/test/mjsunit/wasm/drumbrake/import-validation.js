// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function makeCaller(importedFunction, subtype = false) {
  const builder = new WasmModuleBuilder();
  const signature = builder.addType(kSig_i_i, kNoSuperType, !subtype);
  const imported = builder.addImport("m", "f", signature);
  builder.addTable(kWasmFuncRef, 1, 1);
  builder.addActiveElementSegment(0, wasmI32Const(0), [imported]);
  builder.addFunction("call", signature)
      .addBody([kExprLocalGet, 0, kExprCallFunction, imported])
      .exportFunc();
  builder.addFunction("tailCall", signature)
      .addBody([kExprLocalGet, 0, kExprReturnCall, imported])
      .exportFunc();
  builder.addFunction("callIndirect", signature)
      .addBody([
        kExprLocalGet, 0, kExprI32Const, 0,
        kExprCallIndirect, signature, 0,
      ])
      .exportFunc();
  builder.addFunction("tailCallIndirect", signature)
      .addBody([
        kExprLocalGet, 0, kExprI32Const, 0,
        kExprReturnCallIndirect, signature, 0,
      ])
      .exportFunc();
  return builder.instantiate({m: {f: importedFunction}});
}

for (const subtype of [false, true]) {
  const builder = new WasmModuleBuilder();
  // Give the callee different module-local type and function indices.
  builder.addType(kSig_v_v);
  const supertype = builder.addType(kSig_i_i, kNoSuperType, !subtype);
  const signature = subtype ? builder.addType(kSig_i_i, supertype) : supertype;
  builder.addFunction("unused", kSig_v_v).addBody([]);
  builder.addFunction("callee", signature)
      .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Add,
      ])
      .exportFunc();
  const callee = builder.instantiate();
  const caller = makeCaller(callee.exports.callee, subtype);
  assertEquals(42, caller.exports.call(41));
  assertEquals(42, caller.exports.tailCall(41));
  assertEquals(42, caller.exports.callIndirect(41));
  assertEquals(43, caller.exports.callIndirect(42));
  assertEquals(42, caller.exports.tailCallIndirect(41));
}

(function testJavaScriptImport() {
  let calls = 0;
  const caller = makeCaller(value => {
    ++calls;
    return value + 1;
  });
  assertEquals(42, caller.exports.call(41));
  assertEquals(42, caller.exports.tailCall(41));
  assertEquals(42, caller.exports.callIndirect(41));
  assertEquals(43, caller.exports.callIndirect(42));
  assertEquals(42, caller.exports.tailCallIndirect(41));
  assertEquals(5, calls);
})();
