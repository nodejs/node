// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On riscv64 a Word32 live range is reloaded from its spill slot with a
// sign-extending `Lw` (see CodeGenerator::AssembleMove), so the zero-extended
// register image that ZeroExtendsWord32ToWord64 relies on is not preserved
// across a spill. Eliding the zero-extension of a 32-bit load therefore turned
// `zext32(w)` into `sext32(w)` once the value had been spilled and reloaded,
// which made a memory32 index reach 2GiB below the linear memory.
//
// Three shapes are covered:
//  - `load_extend`: a single Uint32 load that is used both as a Word32 value
//    and zero-extended to Word64. The second use makes the load ineligible for
//    the Lwu fusion, so the zero-extension has to be emitted explicitly.
//  - `phi_extend`: a Phi of two Uint32 loads, the shape from the report. A Phi
//    can never be covered by the load, so only ZeroExtendsWord32ToWord64NoPhis
//    returning false for kWord32 keeps the zero-extension here.
//  - `fused_extend`: a single Uint32 load whose only use is the
//    zero-extension, which VisitChangeUint32ToUint64 fuses into an Lwu. The
//    result is then used as a memory32 index, so this also checks that the
//    fused load is still registered as trapping and out-of-bounds accesses
//    still trap.
//
// The `load_extend` and `phi_extend` results are checked directly instead of
// performing the out-of-bounds access: with a wrong index the access can land
// in unmapped memory and crash with a SIGSEGV instead of raising a wasm trap.
// For `fused_extend` the loaded value itself is the index of the first load,
// so an out-of-bounds access traps in the fused Lwu itself.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.exportMemoryAs('memory');

// Keep the loaded value live across a call so that it has to be spilled, and
// use it both as a Word32 value and as a zero-extended Word64 value.
const callee = builder.addImport('env', 'callee', makeSig([], []));

builder.addFunction('load_extend', makeSig([kWasmI32], [kWasmI64]))
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLocalGet, 0,
      kExprI64LoadMem32U, 0x02, 0x00,  // i64.load32_u
      kExprI32ConvertI64,              // i32.wrap_i64
      kExprLocalSet, 1,
      kExprCallFunction, callee,
      kExprI32Const, 16,
      kExprLocalGet, 1,
      kExprI32StoreMem, 0x02, 0x00,  // Word32 use of the value.
      kExprLocalGet, 1,
      kExprI64UConvertI32,  // Word64 use: i64.extend_i32_u.
    ])
    .exportFunc();

builder.addFunction('phi_extend', makeSig([kWasmI32, kWasmI32], [kWasmI64]))
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLocalGet, 1,  // condition
      kExprIf, kWasmI32,
      kExprLocalGet, 0,
      kExprI64LoadMem32U, 0x02, 0x00,  // Load from offset 0.
      kExprI32ConvertI64,
      kExprElse,
      kExprLocalGet, 0,
      kExprI32Const, 8,
      kExprI32Add,
      kExprI64LoadMem32U, 0x02, 0x00,  // Load from offset 8.
      kExprI32ConvertI64,
      kExprEnd,  // Phi of the two Uint32 loads.
      kExprLocalSet, 2,
      kExprCallFunction, callee,
      kExprI32Const, 16,
      kExprLocalGet, 2,
      kExprI32StoreMem, 0x02, 0x00,  // Word32 use of the value.
      kExprLocalGet, 2,
      kExprI64UConvertI32,  // Word64 use: i64.extend_i32_u.
    ])
    .exportFunc();

builder.addFunction('fused_extend', makeSig([kWasmI32], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprI64LoadMem32U, 0x02, 0x00,  // i64.load32_u
      kExprI32ConvertI64,              // i32.wrap_i64
      kExprI64UConvertI32,  // i64.extend_i32_u, sole use of the load.
      kExprI32ConvertI64,   // i32.wrap_i64, used as a memory32 index.
      kExprI32LoadMem8U, 0x00, 0x00,
    ])
    .exportFunc();

const instance = builder.instantiate({env: {callee: () => {}}});
const view = new DataView(instance.exports.memory.buffer);

// Values with the sign bit set: zero-extending them must not change their
// value, while sign-extending produces a negative (huge unsigned) index.
// The two slots hold distinct values so that phi_extend also verifies that
// the expected branch of the if/else was taken.
view.setUint32(0, 0x80000000, true);
view.setUint32(8, 0xffffffff, true);
assertEquals(0x80000000n, instance.exports.load_extend(0));
assertEquals(0x80000000n, instance.exports.phi_extend(0, 1));
assertEquals(0xffffffffn, instance.exports.phi_extend(0, 0));

// An in-bounds index must load normally. The earlier functions store to
// address 16, so probe a slot they do not touch.
view.setUint8(4, 0x42);
view.setUint32(24, 4, true);
assertEquals(0x42, instance.exports.fused_extend(24));
// A sign-bit index must trap in the fused Lwu itself, not access memory
// below the wasm memory. This exercises the trap-handler bounds-check path.
assertTraps(kTrapMemOutOfBounds, () => instance.exports.fused_extend(0));
// An out-of-bounds offset must also trap in the fused Lwu.
assertTraps(kTrapMemOutOfBounds, () => instance.exports.fused_extend(0x10000));
