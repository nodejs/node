// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let sig_v_i = builder.addType(kSig_v_i);
let cont_v_v = builder.addCont(sig_v_v);
let cont_v_i = builder.addCont(sig_v_i);
let tag_i_i = builder.addTag(kSig_i_i);
let block_sig = builder.addType(
    makeSig([], [kWasmI32, wasmRefType(cont_v_i)]));
let g_cont_v_i = builder.addGlobal(wasmRefNullType(cont_v_i), true, false);

let ilfdsr = [kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmS128,
              kWasmExternRef];
let sig_v_ilfdsr = builder.addType(makeSig(ilfdsr, []));
let cont_v_ilfdsr = builder.addCont(sig_v_ilfdsr);
let tag_ilfdsr_ilfdsr = builder.addTag(makeSig(ilfdsr, ilfdsr));
let block_sig_ilfdsr = builder.addType(
    makeSig([], ilfdsr.concat([wasmRefType(cont_v_ilfdsr)])));

// Use the memory and a global to pass and receive values to and from the
// initial func ref of a stack, until the stack entry wrapper supports
// params/returns:
builder.addMemory(1, 1);
builder.exportMemoryAs('mem');
let g_ref_in = builder.addGlobal(kWasmExternRef, true).exportAs('g_ref_in');
let g_ref_param = builder.addGlobal(kWasmExternRef, true)
  .exportAs('g_ref_param');
let g_ref_out = builder.addGlobal(kWasmExternRef, true).exportAs('g_ref_out');

let suspend_tag0 = builder.addFunction("suspend_tag0", kSig_v_v)
    .addBody([
        kExprI32Const, 6,
        kExprSuspend, tag_i_i,
        kExprI32Const, 7,
        kExprI32Eq,
        kExprBrIf, 0,
        kExprUnreachable,
    ]).exportFunc();
builder.addFunction("test_i32", kSig_v_v)
    .addBody([
        kExprBlock, block_sig,
          kExprRefFunc, suspend_tag0.index,
          kExprContNew, cont_v_v,
          kExprResume, cont_v_v, 1, kOnSuspend, tag_i_i, 0,
          kExprUnreachable,
        kExprEnd,
        kExprGlobalSet, g_cont_v_i.index,
        kExprI32Const, 1, kExprI32Add,
        kExprGlobalGet, g_cont_v_i.index,
        kExprResume, cont_v_i, 0,
    ]).exportFunc();

let suspend_tag_ilfdsr_ilfdsr =
  builder.addFunction("suspend_tag_ilfdsr_ilfdsr", kSig_v_v)
  .addLocals(kWasmI32, 1)
  .addLocals(kWasmI64, 1)
  .addLocals(kWasmF32, 1)
  .addLocals(kWasmF64, 1)
  .addLocals(kWasmS128, 1)
  .addBody([
        // Read an element of each numeric type from memory, pass them via the
        // suspend tag params, receive them back via the tag returns and write
        // them to memory at a different offset.
        // Use a global for the struct ref.
        kExprI32Const, 0, kExprI32LoadMem, 0, 0,
        kExprI32Const, 0, kExprI64LoadMem, 0, 4,
        kExprI32Const, 0, kExprF32LoadMem, 0, 12,
        kExprI32Const, 0, kExprF64LoadMem, 0, 16,
        kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 0, 24,
        kExprGlobalGet, g_ref_in.index,

        kExprSuspend, tag_ilfdsr_ilfdsr,

        kExprGlobalSet, g_ref_out.index,
        kExprLocalSet, 4,
        kExprLocalSet, 3,
        kExprLocalSet, 2,
        kExprLocalSet, 1,
        kExprLocalSet, 0,
        ...wasmI32Const(80), kExprLocalGet, 0, kExprI32StoreMem, 0, 0,
        ...wasmI32Const(80), kExprLocalGet, 1, kExprI64StoreMem, 0, 4,
        ...wasmI32Const(80), kExprLocalGet, 2, kExprF32StoreMem, 0, 12,
        ...wasmI32Const(80), kExprLocalGet, 3, kExprF64StoreMem, 0, 16,
        ...wasmI32Const(80), kExprLocalGet, 4,
        kSimdPrefix, kExprS128StoreMem, 0, 24,
    ]).exportFunc();
let test_ilfdsr = builder.addFunction("test_ilfdsr", kSig_v_v)
  .addLocals(kWasmI32, 1)
  .addLocals(kWasmI64, 1)
  .addLocals(kWasmF32, 1)
  .addLocals(kWasmF64, 1)
  .addLocals(kWasmS128, 1)
  .addLocals(wasmRefType(cont_v_ilfdsr), 1)
  .addBody([
      // Initial resume/suspend:
      kExprBlock, block_sig_ilfdsr,
        kExprRefFunc, suspend_tag_ilfdsr_ilfdsr.index,
        kExprContNew, cont_v_v,
        kExprResume, cont_v_v, 1, kOnSuspend, tag_ilfdsr_ilfdsr, 0,
        kExprUnreachable,
      kExprEnd,
      // Received the params + cont ref. Save the cont ref:
      kExprLocalSet, 5,

      // Write the received params to memory before forwarding them back to
      // the suspended continuation, to check the intermediate state on
      // return.
      kExprGlobalSet, g_ref_param.index,
      kExprLocalSet, 4,
      kExprLocalSet, 3,
      kExprLocalSet, 2,
      kExprLocalSet, 1,
      kExprLocalSet, 0,
      ...wasmI32Const(40), kExprLocalGet, 0, kExprI32StoreMem, 0, 0,
      ...wasmI32Const(40), kExprLocalGet, 1, kExprI64StoreMem, 0, 4,
      ...wasmI32Const(40), kExprLocalGet, 2, kExprF32StoreMem, 0, 12,
      ...wasmI32Const(40), kExprLocalGet, 3, kExprF64StoreMem, 0, 16,
      ...wasmI32Const(40), kExprLocalGet, 4,
      kSimdPrefix, kExprS128StoreMem, 0, 24,

      // Resume and forward the params:
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprGlobalGet, g_ref_param.index,
      kExprLocalGet, 5,
      kExprResume, cont_v_ilfdsr, 0,
  ]).exportFunc();

let instance = builder.instantiate();

(function TestI32() {
  print(arguments.callee.name);
  instance.exports.test_i32();
})();

(function TestManyTypes() {
  print(arguments.callee.name);
  instance.exports.g_ref_in.value = {};
  let mem = new Uint8Array(instance.exports.mem.buffer);
  for (let i = 0; i < 40; ++i) {
    mem[i] = i;
  }
  instance.exports.test_ilfdsr();

  // Check intermediate state:
  for (let i = 0; i < 40; ++i) {
    assertEquals(i, mem[40 + i]);
  }
  assertSame(instance.exports.g_ref_in.value,
             instance.exports.g_ref_param.value);

  // Check final state:
  for (let i = 0; i < 40; ++i) {
    assertEquals(i, mem[80 + i]);
  }
  assertSame(instance.exports.g_ref_in.value,
             instance.exports.g_ref_out.value);
})();
