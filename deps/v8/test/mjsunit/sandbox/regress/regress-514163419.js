// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --expose-gc --compact-on-every-full-gc --no-incremental-marking --no-concurrent-marking

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Pad trusted-space so the next trusted allocation (our table) lands on
// an evacuatable page.
const bigTableBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x04,                    // section kind: Table
  0x05,                    // section length 5
  0x01, 0x70, 0x00,        // table count 1:  funcref no maximum
  0xc0, 0x3e,              // initial size 8000
]);
const bigModule = new WebAssembly.Module(bigTableBytes);
var prePad = [];
for (let i = 0; i < 8; ++i) {
  prePad.push(new WebAssembly.Instance(bigModule));
}

// Build our target module with a table.
let builder = new WasmModuleBuilder();
let sig = builder.addType(makeSig([kWasmI32], [kWasmI32]));
builder.addTable(wasmRefNullType(sig), 1).exportAs("table");
builder.addFunction("dummy", sig)
  .addBody([kExprLocalGet, 0])
  .exportAs("dummy");
let module = new WebAssembly.Module(builder.toBuffer());
let instance = new WebAssembly.Instance(module);
let table = instance.exports.table;

// Get the dispatch table handle and unpublish it.
const dispatchTableHandle =
    Sandbox.readObjectField(table, 'trusted_dispatch_table');
Sandbox.unpublishTrustedHandle(dispatchTableHandle);

// Pad trusted-space so the page holding our table is no longer the
// current linear-allocation page.
var keepAlive = [];
for (let i = 0; i < 8; ++i) {
  keepAlive.push(new WebAssembly.Instance(bigModule));
}

// Run GC compaction. The first GC updates necessary fragmentation
// statistics, and the second GC performs the actual compaction.
gc();
gc();

// Compaction must not republish the object. The following set() call must
// therefore crash.
table.set(0, instance.exports.dummy);

assertUnreachable();
