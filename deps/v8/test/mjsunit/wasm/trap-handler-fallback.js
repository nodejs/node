// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-trap-handler-fallback

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// Make sure we can get at least one guard region if the trap handler is enabled.
(function CanGetGuardRegionTest() {
  print("CanGetGuardRegionTest()");
  const memory = new WebAssembly.Memory({initial: 1});
  if (%IsWasmTrapHandlerEnabled()) {
    assertTrue(%WasmMemoryHasFullGuardRegion(memory));
  }
})();

// This test verifies that when we have too many outstanding memories to get
// another fast memory, we fall back on bounds checking rather than failing.
(function TrapHandlerFallbackTest() {
  print("TrapHandlerFallbackTest()");
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mod", "imported_mem", 1);
  builder.addFunction("load", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
    .exportFunc();
  let memory;
  let instance;
  let instances = [];
  let fallback_occurred = false;
  // Create 135 instances. V8 limits wasm to slightly more than 1 TiB of address
  // space per isolate (see kAddressSpaceLimit in wasm-memory.cc), which allows
  // up to 128 fast memories. As long as we create more than that, we should
  // trigger the fallback behavior.
  for (var i = 0; i < 135 && !fallback_occurred; i++) {
    memory = new WebAssembly.Memory({initial: 1});
    instance = builder.instantiate({mod: {imported_mem: memory}});
    instances.push(instance);

    assertTraps(kTrapMemOutOfBounds, () => instance.exports.load(1 << 20));

    fallback_occurred = !%WasmMemoryHasFullGuardRegion(memory);
  }
  assertTrue(fallback_occurred);
})();

(function TrapHandlerFallbackTestZeroInitialMemory() {
  print("TrapHandlerFallbackTestZeroInitialMemory()");
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mod", "imported_mem", 0);
  builder.addFunction("load", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
    .exportFunc();
  let memory;
  let instance;
  let instances = [];
  let fallback_occurred = false;
  // Create 135 instances. V8 limits wasm to slightly more than 1 TiB of address
  // space per isolate (see kAddressSpaceLimit in wasm-memory.cc), which allows
  // up to 128 fast memories. As long as we create more than that, we should
  // trigger the fallback behavior.
  for (var i = 0; i < 135 && !fallback_occurred; i++) {
    memory = new WebAssembly.Memory({initial: 1});
    instance = builder.instantiate({mod: {imported_mem: memory}});
    instances.push(instance);

    assertTraps(kTrapMemOutOfBounds, () => instance.exports.load(1 << 20));

    fallback_occurred = !%WasmMemoryHasFullGuardRegion(memory);
  }
  assertTrue(fallback_occurred);
})();

(function TrapHandlerFallbackTestGrowFromZero() {
  print("TrapHandlerFallbackTestGrowFromZero()");
  // Create a zero-length memory to make sure the empty backing store is created.
  const zero_memory = new WebAssembly.Memory({initial: 0});

  // Create enough memories to overflow the address space limit
  let memories = []
  for (var i = 0; i < 135; i++) {
    memories.push(new WebAssembly.Memory({initial: 1}));
  }

  // Create a memory for the module. We'll grow this later.
  let memory = new WebAssembly.Memory({initial: 0});

  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mod", "imported_mem", 0);
  builder.addFunction("load", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
    .exportFunc();

  instance = builder.instantiate({mod: {imported_mem: memory}});

  assertTraps(kTrapMemOutOfBounds, () => instance.exports.load(1 << 20));

  try {
    memory.grow(1);
  } catch(e) {
    if (typeof e == typeof new RangeError) {
      return;
    }
    throw e;
  }

  assertTraps(kTrapMemOutOfBounds, () => instance.exports.load(1 << 20));
})();

// Like TrapHandlerFallbackTest, but allows the module to be reused, so we only
// have to recompile once.
(function TrapHandlerFallbackTestReuseModule() {
  print("TrapHandlerFallbackTestReuseModule()");
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mod", "imported_mem", 1);
  builder.addFunction("load", kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
    .exportFunc();
  let memory;
  let instance;
  let instances = [];
  let fallback_occurred = false;
  // Create 135 instances. V8 limits wasm to slightly more than 1 TiB of address
  // space per isolate (see kAddressSpaceLimit in wasm-memory.cc), which allows
  // up to 128 fast memories. As long as we create more than that, we should
  // trigger the fallback behavior.
  const module = builder.toModule();
  for (var i = 0; i < 135 && !fallback_occurred; i++) {
    memory = new WebAssembly.Memory({initial: 1});
    instance = new WebAssembly.Instance(module, {mod: {imported_mem: memory}});
    instances.push(instance);

    assertTraps(kTrapMemOutOfBounds, () => instance.exports.load(1 << 20));

    fallback_occurred = !%WasmMemoryHasFullGuardRegion(memory);
  }
  assertTrue(fallback_occurred);
})();

// Make sure that a bounds checked instance still works when calling an
// imported unchecked function.
(function CallIndirectImportTest() {
  print("CallIndirectImportTest()");
  // Create an unchecked instance that calls a function through an indirect
  // table.
  const instance_a = (() => {
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1, false);
    builder.addFunction("read_mem", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI32LoadMem, 0, 0
      ]).exportAs("read_mem");
    return builder.instantiate();
  })();

  // Create new memories until we get one that is unguarded
  let memories = [];
  let memory;
  for (var i = 0; i < 135; i++) {
    memory = new WebAssembly.Memory({initial: 1});
    memories.push(memory);
    if (!%WasmMemoryHasFullGuardRegion(memory)) {
      break;
    }
  }
  assertFalse(%WasmMemoryHasFullGuardRegion(memory));

  // create a module that imports a function through a table
  const instance_b = (() => {
    const builder = new WasmModuleBuilder();
    builder.addFunction("main", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI32Const, 0,
        kExprCallIndirect, 0, kTableZero
      ]).exportAs("main");
    builder.addImportedTable("env", "table", 1, 1);
    const module = new WebAssembly.Module(builder.toBuffer());
    const table = new WebAssembly.Table({
      element: "anyfunc",
      initial: 1, maximum: 1
    });
    // Hook the new instance's export into the old instance's table.
    table.set(0, instance_a.exports.read_mem);
    return new WebAssembly.Instance(module, {'env': { 'table': table }});
  })();

  // Make sure we get an out of bounds still.
  assertTraps(kTrapMemOutOfBounds, () => instance_b.exports.main(100000));

})();
