// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig = makeSig([kWasmS128, kWasmI32], []);
let tag = builder.addTag(sig);
builder.addExportOfKind("tag", kExternalTag, tag);
builder.addFunction("throw", kSig_v_v).addBody([
    kExprI32Const, 0,
    kSimdPrefix, kExprI32x4Splat,
    kExprI32Const, 12,
    kExprThrow, tag,
]).exportFunc();
let instance = builder.instantiate();
try {
  instance.exports.throw();
} catch (e) {
  assertEquals(12, e.getArg(instance.exports.tag, 1));
}
