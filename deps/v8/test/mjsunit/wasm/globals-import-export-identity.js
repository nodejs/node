// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const global1 = new WebAssembly.Global({value: 'i32', mutable: true}, 14);
const global2 = new WebAssembly.Global({value: 'i32', mutable: true}, 15);
const global3 = new WebAssembly.Global({value: 'i32', mutable: true}, 32);

const builder = new WasmModuleBuilder();

// Two additional globals, so that global-index != export-index.
builder.addImportedGlobal('module', 'global1', kWasmI32, true);
builder.addImportedGlobal('module', 'global2', kWasmI32, true);
const globalIndex =
    builder.addImportedGlobal('module', 'global3', kWasmI32, true);
builder.addExportOfKind('exportedGlobal', kExternalGlobal, globalIndex);

const buffer = builder.toBuffer();

const module = new WebAssembly.Module(buffer);
const instance = new WebAssembly.Instance(module, {
  'module': {
    'global1': global1,
    'global2': global2,
    'global3': global3,
  }
});

assertEquals(global3, instance.exports.exportedGlobal);
