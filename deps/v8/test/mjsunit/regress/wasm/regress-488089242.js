// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --stress-wasm-memory-moving
// Flags: --no-wasm-trap-handler --wasm-enforce-bounds-checks

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const mem = builder.addMemory(1, 220, false);
builder.exportMemoryAs('mem', mem);

const sig_v_v = builder.addType(kSig_v_v);
const cont_index = builder.addCont(sig_v_v);

const grower = builder.addFunction('grower', sig_v_v)
    .addBody([
      kExprI32Const, 1,
      kExprMemoryGrow, mem,
      kExprDrop,
    ]);

builder.addDeclarativeElementSegment([grower.index]);

builder.addFunction('main', kSig_i_v)
    .addBody([
      // resume -> continuation runs memory.grow
      kExprRefFunc, grower.index,
      kExprContNew, cont_index,
      kExprResume, cont_index, 0,

      // post-resume memory ops that may use stale cached memory base
      kExprI32Const, 0,
      ...wasmI32Const(123),
      kExprI32StoreMem, 0, 0,
      kExprI32Const, 0,
      kExprI32LoadMem, 0, 0,
    ])
    .exportFunc();

const instance = builder.instantiate();
const ret = instance.exports.main();
assertEquals(123, ret);
assertEquals(2 * kPageSize, instance.exports.mem.buffer.byteLength);
