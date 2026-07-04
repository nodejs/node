// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-acquire-release

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1, true /* shared */);
builder.exportMemoryAs('memory');
builder.addFunction('main', kSig_v_ii)
  .exportFunc()
  .addBody([
    kExprLocalGet, 0, // index
    kExprLocalGet, 1, // value
    kAtomicPrefix, kExprI32AtomicStore8U,
    0x20, // alignment (bit 5 set to indicate memory order is present)
    kAtomicAcqRel,
    0x00  // offset
  ]);

const instance = builder.instantiate();
instance.exports.main(0, 42);
assertEquals(42, new Uint8Array(instance.exports.memory.buffer)[0]);
