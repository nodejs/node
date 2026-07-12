// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-rab-integration

const v5 = d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const v12 = new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true });
v12.toResizableBuffer();
const v14 = new WasmModuleBuilder();
v14.addImportedMemory("m", "memory", 1, 2, "shared");
const v30 = [kExprLocalGet,0,kExprLocalGet,1,kExprI64Const,0,kAtomicPrefix,kExprI32AtomicWait,2,0];
v14.addFunction("wait", kSig_i_ii).addBody(v30).exportFunc();
const v36 = { memory: v12 };
v14.instantiate({ m: v36 }).exports.wait();
