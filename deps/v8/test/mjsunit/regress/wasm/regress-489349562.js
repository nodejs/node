// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --allow-natives-syntax
// Flags: --stress-wasm-memory-moving --no-wasm-trap-handler
// Flags: --wasm-enforce-bounds-checks

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const mem = builder.addMemory(1, 220, false);
builder.exportMemoryAs('mem', mem);

const sig_v_v = builder.addType(kSig_v_v);
const cont_v_v = builder.addCont(sig_v_v);

const tag = builder.addTag(kSig_v_v);

const inner = builder.addFunction('inner', sig_v_v)
    .addBody([
      kExprSuspend, tag,
      // Memory cache should be reloaded here.
      kExprI32Const, 0,
      ...wasmI32Const(123),
      kExprI32StoreMem, 0, 0,
    ]);

builder.addDeclarativeElementSegment([inner.index]);

builder.addFunction('main', kSig_i_v)
    .addBody([
      kExprBlock, kWasmRef, cont_v_v,
        kExprRefFunc, inner.index,
        kExprContNew, cont_v_v,
        kExprResume, cont_v_v, 1, kOnSuspend, tag, 0,
        kExprUnreachable,
      kExprEnd,

      // Grow memory by 100 pages => forces buffer relocation with
      // --stress-wasm-memory-moving.
      ...wasmI32Const(100),
      kExprMemoryGrow, mem,
      kExprDrop,
      kExprResume, cont_v_v, 0,

      // Read memory[0] from main's perspective (uses FRESH base after
      // EndEffectHandlers reload). Should be 123 if inner's store hit the
      // correct buffer. With the bug, inner wrote to the old buffer, so
      // this reads 0 from the new buffer.
      kExprI32Const, 0,
      kExprI32LoadMem, 0, 0,
    ])
    .exportFunc();

let instance = builder.instantiate();
let ret = instance.exports.main();

assertEquals(123, ret);
assertEquals(101 * kPageSize, instance.exports.mem.buffer.byteLength);
