// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.addDeclarativeElementSegment([0]);
for (let i = 0; i < 4; i++) {
  builder.addFunction(undefined, kSig_v_v).addBody([
    kExprRefFunc, 0,
    kExprDrop,
  ]);
}
builder.addActiveDataSegment(0, [kExprRefFunc, 0, kExprI32Const, 0], []);

assertPromiseResult(
    WebAssembly.compile(builder.toBuffer()), assertUnreachable,
    e => assertInstanceof(e, WebAssembly.CompileError));
