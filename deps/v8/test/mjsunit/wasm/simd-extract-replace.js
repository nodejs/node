// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function ExtractReplace() {
  const replace_configs = [
    { replace: kExprI8x16ReplaceLane,
      lane: 14,
      stack_type: "i32",
      store: kExprI32StoreMem8,
      offset: 14,
    },
    { replace: kExprI16x8ReplaceLane,
      lane: 6,
      stack_type: "i32",
      store: kExprI32StoreMem16,
      offset: 12,
    },
    { replace: kExprI32x4ReplaceLane,
      lane: 3,
      stack_type: "i32",
      store: kExprI32StoreMem,
      offset: 12,
    },
    { replace: kExprF32x4ReplaceLane,
      stack_type: "f32",
      lane: 2,
      store: kExprF32StoreMem,
      offset: 8,
    },
    { replace: kExprI64x2ReplaceLane,
      lane: 1,
      stack_type: "i64",
      store: kExprI64StoreMem,
      offset: 8,
    },
    { replace: kExprF64x2ReplaceLane,
      lane: 0,
      stack_type: "f64",
      store: kExprF64StoreMem,
      offset: 0,
    },
  ];

  const extract_configs = [
    { extract: kExprI8x16ExtractLaneS,
      lane: 13,
      stack_type: "i32",
      load: kExprI32LoadMem8S,
      offset: 13,
    },
    { extract: kExprI8x16ExtractLaneU,
      lane: 8,
      stack_type: "i32",
      load: kExprI32LoadMem8U,
      offset: 8,
    },
    { extract: kExprI16x8ExtractLaneS,
      lane: 4,
      stack_type: "i32",
      load: kExprI32LoadMem16S,
      offset: 8,
    },
    { extract: kExprI16x8ExtractLaneU,
      lane: 5,
      stack_type: "i16",
      load: kExprI32LoadMem16U,
      offset: 10,
    },
    { extract: kExprI32x4ExtractLane,
      lane: 1,
      stack_type: "i32",
      load: kExprI32LoadMem,
      offset: 4,
    },
    { extract: kExprF32x4ExtractLane,
      lane: 0,
      stack_type: "f32",
      load: kExprF32LoadMem,
      offset: 0,
    },
    { extract: kExprI64x2ExtractLane,
      lane: 0,
      stack_type: "i64",
      load: kExprI64LoadMem,
      offset: 0,
    },
    { extract: kExprF64x2ExtractLane,
      lane: 1,
      stack_type: "f64",
      load: kExprF64LoadMem,
      offset: 8,
    },
  ];

  for (let extract_config of extract_configs) {
    print(extract_config.stack_type, extract_config.lane);
    for (let replace_config of replace_configs) {
      if (replace_config.stack_type != extract_config.stack_type) continue;

      print(replace_config.stack_type, replace_config.lane);
      const builder = new WasmModuleBuilder();
      builder.addMemory(1, 1, false);
      builder.exportMemoryAs('memory');

      builder.addFunction('simd_replace', kSig_v_ii)
        .addBody([
          kExprLocalGet, 1,
          kExprLocalGet, 1,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          kExprLocalGet, 0,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          kSimdPrefix, extract_config.extract, extract_config.lane,
          kSimdPrefix, replace_config.replace, replace_config.lane,
          kSimdPrefix, kExprS128StoreMem, 0, 0,
      ]).exportFunc();

      builder.addFunction('scalar_replace', kSig_v_ii)
        .addBody([
          kExprLocalGet, 1,
          kExprLocalGet, 0,
          extract_config.load, 0, extract_config.offset,
          replace_config.store, 0, replace_config.offset,
      ]).exportFunc();

      const module = builder.instantiate();
      // Initialize memory:
      // 2x16-bytes for inputs.
      const memory_view = new Uint8Array(module.exports.memory.buffer);
      for (let i = 0; i < 32; ++i) {
        memory_view[i] = int8_array[i % int8_array.length];
      }
      module.exports.scalar_replace(0, 16);
      const scalar_result = [];
      for (let i = 0; i < 32; ++i) {
        scalar_result[i] = memory_view[i];
      }

      for (let i = 0; i < 32; ++i) {
        memory_view[i] = int8_array[i % int8_array.length];
      }
      module.exports.simd_replace(0, 16);
      for (let i = 0; i < 32; ++i) {
        assertEquals(scalar_result[i], memory_view[i]);
      }
    }
  }
})();
