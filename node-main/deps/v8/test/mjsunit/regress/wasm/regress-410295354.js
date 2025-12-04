// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let sig = makeSig([], [kWasmExnRef]);
let builder = new WasmModuleBuilder();
builder.addImport('q', 'f', sig);
builder.addTable(kWasmAnyFunc, 1, 2);
builder.addActiveElementSegment(0, [kExprI32Const, 0], [0]);
builder.addExportOfKind('table', kExternalTable);
let instance = builder.instantiate({q: {f: () => {}}});
instance.exports.table.grow(1);
