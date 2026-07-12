// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addArray(kWasmI32, true);
builder.addGlobal(wasmRefNullType(0), true, false,
  [...wasmI32Const(1), kGCPrefix, kExprArrayNewDefault, 0]);
builder.addExportOfKind('g', kExternalGlobal, 0);
const instance = builder.instantiate();

Sandbox.corruptObjectField(instance.exports.g, 'raw_type', 0x747);

instance.exports.g.value = {};
