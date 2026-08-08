// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compact-imports

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestCompactImportsByModule_AllKinds() {
  let builder = new WasmModuleBuilder();

  // Mixed types -> Grouped by module name only (0x7F)
  let sig = builder.addType(kSig_v_v);

  // Add one import of each kind
  let group = builder.addImportGroup("mod");
  group.addFunction("f", sig);
  group.addGlobal("g", kWasmI32, false, false);
  group.addTable("t", 1, 10, kWasmFuncRef, false, false);
  group.addMemory("m", 1, 10, false, false);
  group.addTag("tag", sig);

  let buffer = builder.toBuffer();

  let compactImportBModuleCount = 0;
  for (let i = 0; i < buffer.length; ++i) {
    if (buffer[i] === kCompactImportByModule && buffer[i - 1] === 0x00) {
      compactImportBModuleCount++;
    }
  }
  assertEquals(
    1,
    compactImportBModuleCount,
    "Should contain 1 compact import by module grouping",
  );

  let module = new WebAssembly.Module(buffer);
  let imports = WebAssembly.Module.imports(module);
  assertEquals(5, imports.length);

  assertEquals("mod", imports[0].module);
  assertEquals("f", imports[0].name);
  assertEquals("function", imports[0].kind);

  assertEquals("mod", imports[1].module);
  assertEquals("g", imports[1].name);
  assertEquals("global", imports[1].kind);

  assertEquals("mod", imports[2].module);
  assertEquals("t", imports[2].name);
  assertEquals("table", imports[2].kind);

  assertEquals("mod", imports[3].module);
  assertEquals("m", imports[3].name);
  assertEquals("memory", imports[3].kind);

  assertEquals("mod", imports[4].module);
  assertEquals("tag", imports[4].name);
  assertEquals("tag", imports[4].kind);
})();

(function TestCompactImportsByModuleAndType_Tables() {
  let builder = new WasmModuleBuilder();

  // Encoding 2 (0x7E) requires same kind and type.
  let group = builder.addImportedTableGroup(
    "mod",
    1,
    10,
    kWasmFuncRef,
    false,
    false
  );
  group.add("t1");
  group.add("t2");

  let buffer = builder.toBuffer();
  let compactImportBModuleAndTypeCount = 0;
  for (let i = 0; i < buffer.length; ++i) {
    if (buffer[i] === kCompactImportByModuleAndType && buffer[i - 1] === 0x00) {
      compactImportBModuleAndTypeCount++;
    }
  }
  assertEquals(
    1,
    compactImportBModuleAndTypeCount,
    "Should contain 1 compact import by module and type grouping",
  );

  let module = new WebAssembly.Module(buffer);
  let imports = WebAssembly.Module.imports(module);
  assertEquals(2, imports.length);
  assertEquals("mod", imports[0].module);
  assertEquals("t1", imports[0].name);
  assertEquals("table", imports[0].kind);
  assertEquals("mod", imports[1].module);
  assertEquals("t2", imports[1].name);
  assertEquals("table", imports[1].kind);
})();

(function TestCompactImportsByModuleAndType_Memories() {
  let builder = new WasmModuleBuilder();
  let group = builder.addImportedMemoryGroup("mod", 1, 10, false, false);
  group.add("m1");
  group.add("m2");

  let buffer = builder.toBuffer();
  let module = new WebAssembly.Module(buffer);
  let imports = WebAssembly.Module.imports(module);
  assertEquals(2, imports.length);
  assertEquals("m1", imports[0].name);
  assertEquals("memory", imports[0].kind);
  assertEquals("m2", imports[1].name);
  assertEquals("memory", imports[1].kind);
})();

(function TestCompactImportsByModuleAndType_Globals() {
  let builder = new WasmModuleBuilder();
  let group = builder.addImportedGlobalsGroup("mod", kWasmI32, false, false);
  group.add("g1");
  group.add("g2");

  let buffer = builder.toBuffer();
  let module = new WebAssembly.Module(buffer);
  let imports = WebAssembly.Module.imports(module);
  assertEquals(2, imports.length);
  assertEquals("g1", imports[0].name);
  assertEquals("global", imports[0].kind);
  assertEquals("g2", imports[1].name);
  assertEquals("global", imports[1].kind);
})();

(function TestCompactImportsByModuleAndType_Tags() {
  let builder = new WasmModuleBuilder();
  let sig = builder.addType(kSig_v_v);
  let group = builder.addImportedTagGroup("mod", sig);
  group.add("tag1");
  group.add("tag2");

  let buffer = builder.toBuffer();
  let module = new WebAssembly.Module(buffer);
  let imports = WebAssembly.Module.imports(module);
  assertEquals(2, imports.length);
  assertEquals("tag1", imports[0].name);
  assertEquals("tag", imports[0].kind);
  assertEquals("tag2", imports[1].name);
  assertEquals("tag", imports[1].kind);
})();

(function TestCompactImportsByModuleAndType_Functions() {
  let builder = new WasmModuleBuilder();

  let sig = builder.addType(kSig_v_v);
  let group = builder.addImportedFunctionGroup("mod", sig);
  group.add("f1");
  group.add("f2");

  let buffer = builder.toBuffer();
  let module = new WebAssembly.Module(buffer);
  let imports = WebAssembly.Module.imports(module);
  assertEquals(2, imports.length);
  assertEquals("mod", imports[0].module);
  assertEquals("f1", imports[0].name);
  assertEquals("function", imports[0].kind);
  assertEquals("mod", imports[1].module);
  assertEquals("f2", imports[1].name);
  assertEquals("function", imports[1].kind);
})();
