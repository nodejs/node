// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let sig_i_i = builder.addType(kSig_i_i);
let cont_v_v = builder.addCont(sig_v_v);
let cont_i_i = builder.addCont(sig_i_i);
let tag_i_i = builder.addTag(kSig_i_i);
let block_sig = builder.addType(
    makeSig([], [kWasmI32, wasmRefType(cont_i_i)]));
let g_cont_i_i = builder.addGlobal(wasmRefNullType(cont_i_i), true, false);

let ilfdsr = [kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmS128,
              kWasmExternRef];
let sig_ilfdsr_ilfdsr = builder.addType(makeSig(ilfdsr, ilfdsr));
let cont_ilfdsr_ilfdsr = builder.addCont(sig_ilfdsr_ilfdsr);
let tag_ilfdsr_ilfdsr = builder.addTag(makeSig(ilfdsr, ilfdsr));
let block_sig_ilfdsr = builder.addType(
    makeSig([], ilfdsr.concat([wasmRefType(cont_ilfdsr_ilfdsr)])));

// Use the memory and a global to pass and receive values to and from the
// initial func ref of a stack, until the stack entry wrapper supports
// params/returns:
builder.addMemory(1, 1);
builder.exportMemoryAs('mem');
let g_ref_in = builder.addGlobal(kWasmExternRef, true).exportAs('g_ref_in');
let g_ref_param = builder.addGlobal(kWasmExternRef, true)
  .exportAs('g_ref_param');
let g_ref_tag_param = builder.addGlobal(kWasmExternRef, true)
  .exportAs('g_ref_tag_param');
let g_ref_tag_return = builder.addGlobal(kWasmExternRef, true).exportAs('g_ref_tag_return');
let g_ref_cont_return = builder.addGlobal(kWasmExternRef, true).exportAs('g_ref_cont_return');

let suspend_tag0 = builder.addFunction("suspend_tag0", kSig_i_i)
    .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Add,
        kExprSuspend, tag_i_i,
        kExprI32Const, 1,
        kExprI32Add
    ]).exportFunc();
builder.addFunction("test_i32", kSig_i_i)
    .addBody([
        kExprBlock, block_sig,
          kExprLocalGet, 0,
          kExprRefFunc, suspend_tag0.index,
          kExprContNew, cont_i_i,
          kExprResume, cont_i_i, 1, kOnSuspend, tag_i_i, 0,
          kExprUnreachable,
        kExprEnd,
        kExprGlobalSet, g_cont_i_i.index,
        kExprI32Const, 1, kExprI32Add,
        kExprGlobalGet, g_cont_i_i.index,
        kExprResume, cont_i_i, 0,
        kExprI32Const, 1,
        kExprI32Add,
    ]).exportFunc();

let suspend_tag_ilfdsr_ilfdsr =
  builder.addFunction("suspend_tag_ilfdsr_ilfdsr", sig_ilfdsr_ilfdsr)
  .addBody([
      // Save received params to memory/global.
      ...wasmI32Const(40), kExprLocalGet, 0, kExprI32StoreMem, 0, 0,
      ...wasmI32Const(40), kExprLocalGet, 1, kExprI64StoreMem, 0, 4,
      ...wasmI32Const(40), kExprLocalGet, 2, kExprF32StoreMem, 0, 12,
      ...wasmI32Const(40), kExprLocalGet, 3, kExprF64StoreMem, 0, 16,
      ...wasmI32Const(40), kExprLocalGet, 4,
      kSimdPrefix, kExprS128StoreMem, 0, 24,
      kExprLocalGet, 5, kExprGlobalSet, g_ref_param.index,

      // Forward params to the suspend tag.
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprLocalGet, 5,
      kExprSuspend, tag_ilfdsr_ilfdsr,

      // Receive tag returns.
      kExprGlobalSet, g_ref_tag_return.index,
      kExprLocalSet, 4,
      kExprLocalSet, 3,
      kExprLocalSet, 2,
      kExprLocalSet, 1,
      kExprLocalSet, 0,

      // Save tag returns to memory/global.
      ...wasmI32Const(120), kExprLocalGet, 0, kExprI32StoreMem, 0, 0,
      ...wasmI32Const(120), kExprLocalGet, 1, kExprI64StoreMem, 0, 4,
      ...wasmI32Const(120), kExprLocalGet, 2, kExprF32StoreMem, 0, 12,
      ...wasmI32Const(120), kExprLocalGet, 3, kExprF64StoreMem, 0, 16,
      ...wasmI32Const(120), kExprLocalGet, 4,
      kSimdPrefix, kExprS128StoreMem, 0, 24,

      // Finally, return the values.
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprGlobalGet, g_ref_tag_return.index,
  ]).exportFunc();
let test_ilfdsr = builder.addFunction("test_ilfdsr", kSig_v_v)
  .addLocals(kWasmI32, 1)
  .addLocals(kWasmI64, 1)
  .addLocals(kWasmF32, 1)
  .addLocals(kWasmF64, 1)
  .addLocals(kWasmS128, 1)
  .addLocals(wasmRefType(cont_ilfdsr_ilfdsr), 1)
  .addBody([
      // Initial resume/suspend:
      kExprBlock, block_sig_ilfdsr,
        kExprI32Const, 0, kExprI32LoadMem, 0, 0,
        kExprI32Const, 0, kExprI64LoadMem, 0, 4,
        kExprI32Const, 0, kExprF32LoadMem, 0, 12,
        kExprI32Const, 0, kExprF64LoadMem, 0, 16,
        kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 0, 24,
        kExprGlobalGet, g_ref_in.index,
        kExprRefFunc, suspend_tag_ilfdsr_ilfdsr.index,
        kExprContNew, cont_ilfdsr_ilfdsr,
        kExprResume, cont_ilfdsr_ilfdsr, 1, kOnSuspend, tag_ilfdsr_ilfdsr, 0,
        kExprUnreachable,
      kExprEnd,

      // Receive cont and tag params.
      kExprLocalSet, 5,
      kExprGlobalSet, g_ref_tag_param.index,
      kExprLocalSet, 4,
      kExprLocalSet, 3,
      kExprLocalSet, 2,
      kExprLocalSet, 1,
      kExprLocalSet, 0,

      // Save tag params to memory/global.
      ...wasmI32Const(80), kExprLocalGet, 0, kExprI32StoreMem, 0, 0,
      ...wasmI32Const(80), kExprLocalGet, 1, kExprI64StoreMem, 0, 4,
      ...wasmI32Const(80), kExprLocalGet, 2, kExprF32StoreMem, 0, 12,
      ...wasmI32Const(80), kExprLocalGet, 3, kExprF64StoreMem, 0, 16,
      ...wasmI32Const(80), kExprLocalGet, 4,
      kSimdPrefix, kExprS128StoreMem, 0, 24,

      // Resume with the same params.
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprGlobalGet, g_ref_tag_param.index,
      kExprLocalGet, 5,
      kExprResume, cont_ilfdsr_ilfdsr, 0,

      // Receive return values.
      kExprGlobalSet, g_ref_cont_return.index,
      kExprLocalSet, 4,
      kExprLocalSet, 3,
      kExprLocalSet, 2,
      kExprLocalSet, 1,
      kExprLocalSet, 0,

      // Save return values to memory/global.
      ...wasmI32Const(160), kExprLocalGet, 0, kExprI32StoreMem, 0, 0,
      ...wasmI32Const(160), kExprLocalGet, 1, kExprI64StoreMem, 0, 4,
      ...wasmI32Const(160), kExprLocalGet, 2, kExprF32StoreMem, 0, 12,
      ...wasmI32Const(160), kExprLocalGet, 3, kExprF64StoreMem, 0, 16,
      ...wasmI32Const(160), kExprLocalGet, 4,
      kSimdPrefix, kExprS128StoreMem, 0, 24,
  ]).exportFunc();

let instance = builder.instantiate();

(function TestI32() {
  print(arguments.callee.name);
  // Param is incremented 4 times, once after each stack switch: start, suspend,
  // resume, return.
  assertEquals(5, instance.exports.test_i32(1));
})();

(function TestManyTypes() {
  print(arguments.callee.name);
  instance.exports.g_ref_in.value = {};
  let mem = new Uint8Array(instance.exports.mem.buffer);
  for (let i = 0; i < 40; ++i) {
    mem[i] = i;
  }
  instance.exports.test_ilfdsr();

  // Check initial params:
  for (let i = 0; i < 40; ++i) {
    assertEquals(i, mem[40 + i]);
  }
  assertSame(instance.exports.g_ref_in.value,
             instance.exports.g_ref_param.value);

  // Check tag params:
  for (let i = 0; i < 40; ++i) {
    assertEquals(i, mem[80 + i]);
  }
  assertSame(instance.exports.g_ref_in.value,
             instance.exports.g_ref_tag_param.value);

  // Check tag returns:
  for (let i = 0; i < 40; ++i) {
    assertEquals(i, mem[120 + i]);
  }
  assertSame(instance.exports.g_ref_in.value,
             instance.exports.g_ref_tag_return.value);

  // Check cont returns:
  for (let i = 0; i < 40; ++i) {
    assertEquals(i, mem[160 + i]);
  }
  assertSame(instance.exports.g_ref_in.value,
             instance.exports.g_ref_cont_return.value);
})();
