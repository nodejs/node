// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1);
builder.addFunction(undefined, kSig_v_i) .addBodyWithEnd([
    kExprI32Const, 1, kExprMemoryGrow, kMemoryZero, kNumericPrefix]);
// Intentionally add just a numeric opcode prefix without the index byte.

const b = builder.toBuffer();
WebAssembly.compile(b).then(() => assertUnreachable(), () => { /* ignore */ })
