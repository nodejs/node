// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-fp16

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var kMemSize = 65536;

function testOOBThrowsF16() {
  var builder = new WasmModuleBuilder();
  const kSig_f_ii = makeSig([kWasmI32, kWasmI32], [kWasmF32]);

  builder.addMemory(1, 1);
  builder.addFunction("get", kSig_f_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kNumericPrefix, kExprF32LoadF16, 0, 0,
      kNumericPrefix, kExprF32StoreF16, 0, 0,
      kExprLocalGet, 1,
      kNumericPrefix, kExprF32LoadF16, 0, 0,
    ].flat()).exportFunc();

  var module = builder.instantiate();

  let read = offset => module.exports.get(0, offset);
  let write = offset =>  module.exports.get(offset, 0);

  assertEquals(0, read(65532));
  assertEquals(0, write(65532));

  // Note that this test might be run concurrently in multiple Isolates, which
  // makes an exact comparison of the expected trap count unreliable. But is is
  // still possible to check the lower bound for the expected trap count.
  for (let offset = kMemSize - 1; offset <= kMemSize; offset++) {
    const trap_count = %GetWasmRecoveredTrapCount();
    assertTraps(kTrapMemOutOfBounds, () => read(offset));
    assertTraps(kTrapMemOutOfBounds, () => write(offset));
    if (%IsWasmTrapHandlerEnabled()) {
      if (%IsWasmPartialOOBWriteNoop()) {
        assertTrue(trap_count + 2 <= %GetWasmRecoveredTrapCount());
      } else {
        assertTrue(trap_count + 1 <= %GetWasmRecoveredTrapCount());
      }
    }
  }
}

testOOBThrowsF16();
