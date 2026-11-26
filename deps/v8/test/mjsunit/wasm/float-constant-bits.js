// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-inlining

// f32 and f64 constants passed as call arguments keep their exact bits in
// Liftoff and in optimized code: the ones a single instruction can build on
// some targets (riscv64's Zfa fli.s and fli.d) and their neighbours that it
// cannot, NaN payloads included.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function constBytes(opcode, bits, size) {
  const bytes = [opcode];
  for (let i = 0n; i < BigInt(size); i++) {
    bytes.push(Number((bits >> (8n * i)) & 0xffn));
  }
  return bytes;
}

const kF64Bits = [
  0x3ff0000000000000n,  // 1
  0xbff0000000000000n,  // -1
  0x3fe0000000000000n,  // 0.5
  0x40f0000000000000n,  // 65536
  0x7ff0000000000000n,  // Infinity
  0x7ff8000000000000n,  // canonical NaN
  0x0010000000000000n,  // DBL_MIN, fli.d entry 1
  0x3810000000000000n,  // FLT_MIN, not an fli.d entry
  0x3ef0000000000000n,  // 2^-16
  0xbfe0000000000000n,  // -0.5
  0xfff0000000000000n,  // -Infinity
  0x8000000000000000n,  // -0
  0x3fb999999999999an,  // 0.1
  0x7ff8000000000001n,  // quiet NaN with a payload
  0x7ff4000000000000n,  // signaling NaN
  0xfff8000000000000n,  // negative quiet NaN
];

const kF32Bits = [
  0x3f800000n,  // 1
  0xbf800000n,  // -1
  0x3f000000n,  // 0.5
  0x47800000n,  // 65536
  0x7f800000n,  // Infinity
  0x7fc00000n,  // canonical NaN
  0x00800000n,  // FLT_MIN, fli.s entry 1
  0x37800000n,  // 2^-16
  0xbf000000n,  // -0.5
  0xff800000n,  // -Infinity
  0x80000000n,  // -0
  0x3dcccccdn,  // 0.1
  0x7fc00001n,  // quiet NaN with a payload
  0x7fa00000n,  // signaling NaN
  0xffc00000n,  // negative quiet NaN
];

(function TestFloatConstantBits() {
  const builder = new WasmModuleBuilder();
  const reinterpretF64 =
      builder.addFunction('reinterpretF64', makeSig([kWasmF64], [kWasmI64]))
          .addBody([kExprLocalGet, 0, kExprI64ReinterpretF64]);
  const reinterpretF32 = builder.addFunction('reinterpretF32', kSig_i_f)
      .addBody([kExprLocalGet, 0, kExprI32ReinterpretF32]);
  kF64Bits.forEach((bits, i) => {
    builder.addFunction(`f64_${i}`, kSig_l_v)
        .addBody([
          ...constBytes(kExprF64Const, bits, 8),
          kExprCallFunction, reinterpretF64.index,
        ])
        .exportFunc();
  });
  kF32Bits.forEach((bits, i) => {
    builder.addFunction(`f32_${i}`, kSig_i_v)
        .addBody([
          ...constBytes(kExprF32Const, bits, 4),
          kExprCallFunction, reinterpretF32.index,
        ])
        .exportFunc();
  });
  const exports = builder.instantiate().exports;

  function check(name, bits, expected) {
    const f = exports[name];
    assertEquals(expected, f(), `${name} (${bits.toString(16)})`);
    %WasmTierUpFunction(f);
    assertEquals(expected, f(), `${name} (${bits.toString(16)}) optimized`);
  }
  kF64Bits.forEach(
      (bits, i) => check(`f64_${i}`, bits, BigInt.asIntN(64, bits)));
  kF32Bits.forEach(
      (bits, i) => check(`f32_${i}`, bits, Number(BigInt.asIntN(32, bits))));
})();
