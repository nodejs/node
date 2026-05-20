// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function LoadReplace() {
  const store_configs = [
    { store: kExprI32StoreMem8, stack_type: "i32", size: 1 },
    { store: kExprI32StoreMem16, stack_type: "i32", size: 2 },
    { store: kExprI32StoreMem, stack_type: "i32", size: 4 },
    { store: kExprI64StoreMem, stack_type: "i64", size: 8 },
    { store: kExprF32StoreMem, stack_type: "f32", size: 4 },
    { store: kExprF64StoreMem, stack_type: "f64", size: 8 },
  ];

  const load_configs = [
    { load: kExprI32LoadMem8S, stack_type: "i32", offset: 1 },
    { load: kExprI32LoadMem8U, stack_type: "i32", offset: 2 },
    { load: kExprI32LoadMem16S, stack_type: "i32", offset: 3 },
    { load: kExprI32LoadMem16U, stack_type: "i32", offset: 4 },
    { load: kExprI32LoadMem, stack_type: "i32", offset: 5 },
    { load: kExprI64LoadMem, stack_type: "i64", offset: 6 },
    { load: kExprF32LoadMem, stack_type: "f32", offset: 7 },
    { load: kExprF64LoadMem, stack_type: "f64", offset: 8 },
  ];

  const replace_configs = [
    { replace: kExprI8x16ReplaceLane, stack_type: "i32", size: 1, lane: 14 },
    { replace: kExprI16x8ReplaceLane, stack_type: "i32", size: 2, lane: 5 },
    { replace: kExprI32x4ReplaceLane, stack_type: "i32", size: 4, lane: 2 },
    { replace: kExprI64x2ReplaceLane, stack_type: "i64", size: 8, lane: 1 },
    { replace: kExprF32x4ReplaceLane, stack_type: "f32", size: 4, lane: 3 },
    { replace: kExprF64x2ReplaceLane, stack_type: "f64", size: 8, lane: 0 },
  ];

  for (let store_config of store_configs) {
    for (let load_config of load_configs) {
      if (load_config.stack_type != store_config.stack_type) continue;

      for (let replace_config of replace_configs) {
        if (replace_config.stack_type != load_config.stack_type) continue;
        if (replace_config.size != store_config.size) continue;

        print(replace_config.stack_type, replace_config.lane, load_config.offset);

        const builder = new WasmModuleBuilder();
        builder.addMemory(1, 1, false);
        builder.exportMemoryAs('memory');
        builder.addFunction('simd_replace', kSig_v_ii)
          .addBody([
            kExprLocalGet, 1,
            kExprLocalGet, 1,
            kSimdPrefix, kExprS128LoadMem, 0, 0,
            kExprLocalGet, 0,
            load_config.load, 0, load_config.offset,
            kSimdPrefix, replace_config.replace, replace_config.lane,
            kSimdPrefix, kExprS128StoreMem, 0, 0,
        ]).exportFunc();

        builder.addFunction('scalar_replace', kSig_v_ii)
          .addBody([
            kExprLocalGet, 1,
            kExprLocalGet, 0,
            load_config.load, 0, load_config.offset,
            store_config.store, 0, (replace_config.size * replace_config.lane),
        ]).exportFunc();

        const module = builder.instantiate();
        // Initialize memory:
        const memory_view = new Uint8Array(module.exports.memory.buffer);
        for (let i = 0; i < 512; ++i) {
          memory_view[i] = int8_array[i % int8_array.length];
        }
        module.exports.scalar_replace(0, 256);
        const scalar_result = [];
        for (let i = 0; i < 512; ++i) {
          scalar_result[i] = memory_view[i];
        }

        for (let i = 0; i < 512; ++i) {
          memory_view[i] = int8_array[i % int8_array.length];
        }
        module.exports.simd_replace(0, 256);
        for (let i = 0; i < 512; ++i) {
          assertEquals(scalar_result[i], memory_view[i]);
        }
      }
    }
  }
})();
