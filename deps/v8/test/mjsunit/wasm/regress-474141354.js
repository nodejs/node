// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff
// Flags: --turbo-instruction-scheduling --turbo-stress-instruction-scheduling

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function MopsMemoryFillOobTrapNoWrite() {
  print('MopsMemoryFillOobTrapNoWrite');

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('fill', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (value)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryFill, 0     // memory.fill dstmem=0
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const memory = new Uint8Array(instance.exports.memory.buffer);
  memory.fill(0x11);

  const kPageSize = 65536;
  const dst = kPageSize - 8;
  const size = 256;
  const value = 0x22;

  let trapped = false;
  try {
    instance.exports.fill(dst, value, size);
  } catch (e) {
    trapped = true;
  }
  if (!trapped) {
    throw new Error('expected trap');
  }

  for (let i = 0; i < 8; ++i) {
    if (memory[dst + i] !== 0x11) {
      throw new Error('memory modified after OOB trap');
    }
  }
})();
