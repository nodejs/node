// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Regression test for https://crbug.com/557106111
//
// TryGetExtendIntToF32x4Info in src/compiler/turboshaft/wasm-revec-reducer.cc
// matches the pattern
//
//   f32x4.replace_lane 3 (f32x4.replace_lane 2 (f32x4.replace_lane 1
//     (f32x4.splat (f32.convert_i32 (i8x16.extract_lane 0 v))) ...)))
//
// and rewrites it into a single integer-to-float widening. It only checked
// that lane 0 was *some* Simd128SplatOp, never that its kind was f32x4. An
// f64x2.splat passed the remaining checks as well, because the ChangeOp
// comparison only looks at the kind (kSignedToFloat) and not at the
// representation, so Int32->Float64 compares equal to the lanes'
// Int32->Float32.
//
// f64x2.splat(1.0) leaves the low 32 bits of lane 0 as 0, but the bogus
// rewrite produced float(1) == 0x3f800000 there instead.

let builder = new WasmModuleBuilder();
builder.addMemory(1, 1, true);
builder.exportMemoryAs('memory', 0);

let main = builder.addFunction('main', kSig_v_v).exportFunc();
main.addLocals(kWasmS128, 1);

main.addBody([
  // v = i8x16.splat(1)
  kExprI32Const, 1,
  kSimdPrefix, kExprI8x16Splat,
  kExprLocalSet, 0,

  // Store at offset 0. Lane 0 comes from an f64x2.splat rather than an
  // f32x4.splat, so this is not an int-to-f32x4 extend.
  kExprI32Const, 0,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 0,
  kExprF64SConvertI32,
  kSimdPrefix, kExprF64x2Splat,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 1,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4ReplaceLane, 1,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 2,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4ReplaceLane, 2,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 3,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4ReplaceLane, 3,

  kSimdPrefix, kExprS128StoreMem, 0, 0,

  // Store at offset 16. This one is a real int-to-f32x4 extend, and is
  // adjacent to the first store so that the two get packed together.
  kExprI32Const, 0,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 4,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4Splat,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 5,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4ReplaceLane, 1,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 6,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4ReplaceLane, 2,

  kExprLocalGet, 0,
  kSimdPrefix, kExprI8x16ExtractLaneS, 7,
  kExprF32SConvertI32,
  kSimdPrefix, kExprF32x4ReplaceLane, 3,

  kSimdPrefix, kExprS128StoreMem, 0, 16,
]);

const instance = builder.instantiate();
instance.exports.main();

let mem = new Uint32Array(instance.exports.memory.buffer);
// Low 32 bits of the f64 1.0 splatted into lane 0. The bug replaced this with
// float(1) == 0x3f800000. The high 32 bits of that f64 are overwritten by the
// f32x4.replace_lane 1 below, so only lane 0 survives from the f64x2.splat.
assertEquals(0, mem[0]);
assertEquals(0x3f800000, mem[1]);
assertEquals(0x3f800000, mem[2]);
assertEquals(0x3f800000, mem[3]);
