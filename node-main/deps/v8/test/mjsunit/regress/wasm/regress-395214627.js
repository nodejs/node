// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

assertThrows(
    () => new WebAssembly.Exception(new WebAssembly.Tag({ parameters: ["v128"] }), [0]),
    TypeError);

let builder = new WasmModuleBuilder();
let tag = builder.addTag(makeSig([kWasmS128], []));
builder.addExportOfKind("tag", kExternalTag, tag);
builder.addFunction("main", kSig_v_v).addBody([
    kSimdPrefix, kExprS128Const, ...(new Array(16).fill(0)),
    kExprThrow, tag
]).exportFunc();
let instance = builder.instantiate();
try {
  instance.exports.main();
  assertUnreachable();
} catch (e) {
  assertTrue(e instanceof WebAssembly.Exception);
  assertThrows(() => e.getArg(instance.exports.tag, 0), TypeError);
}
