// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-revectorize

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $array1 = builder.addArray(kWasmI16, true, kNoSuperType, true);
let sigMain = builder.addType(kSig_i_iii);
let sigCast =
  builder.addType(makeSig([kWasmExternRef], [wasmRefType(kWasmExternRef)]));
let cast = builder.addImport('wasm:js-string', 'cast', sigCast);
let main = builder.addFunction(undefined, sigMain).exportAs('main');

main.addBody([
    ...wasmF32Const(0.0),
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kSimdPrefix, kExprI8x16SConvertI16x8,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kSimdPrefix, kExprI8x16SConvertI16x8,
    kSimdPrefix, kExprI8x16SConvertI16x8,
    ...SimdInstr(kExprI64x2SConvertI32x4High),
    ...SimdInstr(kExprF64x2Abs),
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprRefNull, $array1,
    kGCPrefix, kExprArrayLen,
    kSimdPrefix, kExprI8x16ReplaceLane, 0,
    kSimdPrefix, kExprI8x16SConvertI16x8,
    kExprRefNull, kExternRefCode,
    kExprCallFunction, cast,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefTestNull, $array1,
    ...SimdInstr(kExprI16x8ShrS),
    ...SimdInstr(kExprI64x2SConvertI32x4High),
    ...SimdInstr(kExprF64x2Abs),
    kSimdPrefix, kExprI8x16SConvertI16x8,
    kSimdPrefix, kExprF32x4ExtractLane, 0,
    kExprF32Lt,
  ]);

let kBuiltins = {builtins: ['js-string']};
const instance = builder.instantiate({}, kBuiltins);
assertTraps(kTrapNullDereference, () => instance.exports.main(1, 2, 3));
