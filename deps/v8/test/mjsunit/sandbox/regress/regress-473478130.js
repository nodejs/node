// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addTag(makeSig([kWasmI32, kWasmI32, kWasmI32,], []));
builder.addExportOfKind('t', kExternalTag, 0);
const instance = builder.instantiate();

const sbx = new DataView(new Sandbox.MemoryView(0, 4294967296));
sbx.setUint32(Sandbox.getAddressOf(instance.exports.t) - 0xc, parseInt(11111100111, 2), true);
new WebAssembly.Exception(instance.exports.t, [1, 1, 1]);
