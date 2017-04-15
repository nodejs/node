// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// This test verifies that when instances are exported, Gc'ed, the other
// instances in the chain still maintain a consistent view of the memory.
(function ValidateSharedInstanceMemory() {
  print("ValidateSharedInstanceMemory");
  let memory = new WebAssembly.Memory({initial: 5, maximum: 100});
  var builder = new WasmModuleBuilder();
  builder.addImportedMemory("mod", "imported_mem");
  builder.addFunction("mem_size", kSig_i_v)
    .addBody([kExprMemorySize, kMemoryZero])
    .exportFunc();
  builder.addFunction("grow", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprGrowMemory, kMemoryZero])
    .exportFunc();
  var instances = [];
  for (var i = 0; i < 5; i++) {
    instances.push(builder.instantiate({mod: {imported_mem: memory}}));
  }
  function grow_instance_0(pages) { return instances[0].exports.grow(pages); }
  function grow_instance_1(pages) { return instances[1].exports.grow(pages); }
  function grow_instance_2(pages) { return instances[2].exports.grow(pages); }
  function grow_instance_3(pages) { return instances[3].exports.grow(pages); }
  function grow_instance_4(pages) { return instances[4].exports.grow(pages); }

  var start_index = 0;
  var end_index = 5;
  function verify_mem_size(expected_pages) {
    assertEquals(expected_pages*kPageSize, memory.buffer.byteLength);
    for (var i = start_index; i < end_index; i++) {
      assertEquals(expected_pages, instances[i].exports.mem_size());
    }
  }

  // Verify initial memory size of all instances, grow and verify that all
  // instances are updated correctly.
  verify_mem_size(5);
  assertEquals(5, memory.grow(6));
  verify_mem_size(11);

  instances[1] = null;
  gc();

  // i[0] - i[2] - i[3] - i[4]
  start_index = 2;
  verify_mem_size(11);
  assertEquals(11, instances[0].exports.mem_size());
  assertEquals(11, grow_instance_2(10));
  assertEquals(21*kPageSize, memory.buffer.byteLength);
  verify_mem_size(21);
  assertEquals(21, instances[0].exports.mem_size());

  instances[4] = null;
  gc();

  // i[0] - i[2] - i[3]
  assertEquals(21, instances[0].exports.mem_size());
  assertEquals(21, instances[2].exports.mem_size());
  assertEquals(21, instances[3].exports.mem_size());
  assertEquals(21, memory.grow(2));
  assertEquals(23*kPageSize, memory.buffer.byteLength);
  assertEquals(23, instances[0].exports.mem_size());
  assertEquals(23, instances[2].exports.mem_size());
  assertEquals(23, instances[3].exports.mem_size());

  instances[0] = null;
  gc();

  // i[2] - i[3]
  assertEquals(23, instances[2].exports.mem_size());
  assertEquals(23, instances[3].exports.mem_size());
  assertEquals(23, grow_instance_3(5));
  assertEquals(28*kPageSize, memory.buffer.byteLength);
  assertEquals(28, instances[2].exports.mem_size());
  assertEquals(28, instances[3].exports.mem_size());

  // Instantiate a new instance and verify that it can be grown correctly.
  instances.push(builder.instantiate({mod: {imported_mem: memory}}));
  function grow_instance_5(pages) { return instances[5].exports.grow(pages); }

  // i[2] - i[3] - i[5]
  assertEquals(28, instances[2].exports.mem_size());
  assertEquals(28, instances[3].exports.mem_size());
  assertEquals(28, instances[5].exports.mem_size());
  assertEquals(28, grow_instance_5(2));
  assertEquals(30*kPageSize, memory.buffer.byteLength);
  assertEquals(30, instances[2].exports.mem_size());
  assertEquals(30, instances[3].exports.mem_size());
  assertEquals(30, instances[5].exports.mem_size());
  assertEquals(30, memory.grow(5));
  assertEquals(35*kPageSize, memory.buffer.byteLength);
  assertEquals(35, instances[2].exports.mem_size());
  assertEquals(35, instances[3].exports.mem_size());
  assertEquals(35, instances[5].exports.mem_size());

})();
