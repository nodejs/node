// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --liftoff-only
// Force all functions (the first 8, technically) to generate debug code.
// Flags: --wasm-debug-mask-for-testing=255

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function testGCInLoopStackCheck() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);

  const imp_index = builder.addImport('q', 'triggerStackCheck', kSig_v_v);

  const kIndex = 0;
  const kValue = 11;

  // This is a regression test for https://crbug.com/1222648:
  // Add a memory instruction before the loop, to get the memory start cached.
  // Then add a memory instruction inside the loop to make use of the cached
  // memory start.
  const main =
      builder.addFunction('main', kSig_i_v)
          .addBody([
            kExprCallFunction, imp_index,     // schedule stack check
            kExprI32Const,     kIndex,        // i32.const kIndex
            kExprI32Const,     kValue,        // i32.const kValue
            kExprI32StoreMem,  0,         0,  // i32.store align=0 offset=0
            kExprLoop,         kWasmVoid,     // loop
            kExprI32Const,     kIndex,        // i32.const kIndex
            kExprI32LoadMem,   0,         0,  // i32.load align=0 offset=0
            kExprReturn,                      // return
            kExprEnd,                         // end loop
            kExprUnreachable,                 // unreachable
          ])
          .exportFunc();

  const instance = builder.instantiate(
      {q: {triggerStackCheck: () => %ScheduleGCInStackCheck()}});

  assertEquals(kValue, instance.exports.main());
})();
