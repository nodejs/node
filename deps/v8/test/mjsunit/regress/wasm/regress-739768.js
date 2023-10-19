// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Flags: --wasm-lazy-compilation

let builder0 = new WasmModuleBuilder();
builder0.setName('module_0');
let sig_index = builder0.addType(kSig_i_v);
builder0.addFunction('main', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,  // --
      kExprCallIndirect, sig_index, kTableZero
    ])  // --
    .exportAs('main');
builder0.setTableBounds(3, 3);
builder0.addExportOfKind('table', kExternalTable);
let module0 = new WebAssembly.Module(builder0.toBuffer());
let instance0 = new WebAssembly.Instance(module0);

let builder1 = new WasmModuleBuilder();
builder1.setName('module_1');
builder1.addFunction('main', kSig_i_v).addBody([kExprUnreachable]);
builder1.addImportedTable('z', 'table');
builder1.addActiveElementSegment(0, wasmI32Const(0), [0]);
let module1 = new WebAssembly.Module(builder1.toBuffer());
let instance1 =
    new WebAssembly.Instance(module1, {z: {table: instance0.exports.table}});
assertThrows(
    () => instance0.exports.main(0), WebAssembly.RuntimeError, 'unreachable');
