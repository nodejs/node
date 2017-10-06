// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc --verify-heap

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

const verifyHeap = gc;
let globalCounter = 10000000;

function testProperties(obj) {
  for (let i = 0; i < 3; i++) {
    obj.x = 1001;
    assertEquals(1001, obj.x);

    obj.y = "old";
    assertEquals("old", obj.y);

    delete obj.y;
    assertEquals("undefined", typeof obj.y);

    let uid = globalCounter++;
    let fresh = "f_" + uid;

    obj.z = fresh;
    assertEquals(fresh, obj.z);

    obj[fresh] = uid;
    assertEquals(uid, obj[fresh]);

    verifyHeap();

    assertEquals(1001, obj.x);
    assertEquals(fresh, obj.z);
    assertEquals(uid, obj[fresh]);
  }

  // These properties are special for JSFunctions.
  Object.defineProperty(obj, 'name', {value: "crazy"});
  Object.defineProperty(obj, 'length', {value: 999});
}

function minus18(x) { return x - 18; }
function id(x) { return x; }

function printName(when, f) {
  print("    " + when + ": name=" + f.name + ", length=" + f.length);
}

(function ExportedFunctionTest() {
  print("ExportedFunctionTest");

  print("  instance 1, exporting");
  var builder = new WasmModuleBuilder();
  builder.addFunction("exp", kSig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprCallFunction, 0])
    .exportAs("exp");
  let module1 = builder.toModule();
  let instance1 = new WebAssembly.Instance(module1);
  let g = instance1.exports.exp;

  testProperties(g);

  // The WASM-internal fields of {g} are only inspected when {g} is
  // used as an import into another instance.
  print("  instance 2, importing");
  var builder = new WasmModuleBuilder();
  builder.addImport("imp", "func", kSig_i_i);
  let module2 = builder.toModule();
  let instance2 = new WebAssembly.Instance(module2, {imp: {func: g}});

  testProperties(g);
})();

(function ImportReexportChain() {
  print("ImportReexportChain");

  var f = id;

  for (let i = 0; i < 5; i++) {
    let builder = new WasmModuleBuilder();
    builder.addImport("imp", "func", kSig_i_i);
    builder.addExport("exp", 0);
    let module = builder.toModule();
    let instance = new WebAssembly.Instance(module, {imp: {func: f}});
    let g = instance.exports.exp;
    assertInstanceof(g, Function);
    printName("before", g);
    testProperties(g);
    printName(" after", g);

    // The WASM-internal fields of {g} are only inspected when {g} is
    // used as an import into another instance. Use {g} as the import
    // the next time through the loop.
    f = g;
  }
})();


(function ModuleTest() {
  for (f of [x => (x + 19 + globalCounter), minus18]) {
    print("ModuleTest");

    let builder = new WasmModuleBuilder();
    builder.addImport("m", "f", kSig_i_i);
    builder.addFunction("main", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprCallFunction, 0])
      .exportAs("main");
    builder.addMemory(1, 1, false)
      .exportMemoryAs("memory")

    let module = builder.toModule();
    testProperties(module);

    for (let i = 0; i < 3; i++) {
      print("  instance " + i);
      let instance = new WebAssembly.Instance(module, {m: {f: f}});
      testProperties(instance);

      print("  memory   " + i);
      let m = instance.exports.memory;
      assertInstanceof(m, WebAssembly.Memory);
      testProperties(m);

      print("  function " + i);
      let g = instance.exports.main;
      assertInstanceof(g, Function);
      printName("before", g);
      testProperties(g);
      printName(" after", g);
      assertInstanceof(g, Function);
      testProperties(g);
      for (let j = 10; j < 15; j++) {
        assertEquals(f(j), g(j));
      }
      verifyHeap();
      // The WASM-internal fields of {g} are only inspected when {g} is
      // used as an import into another instance. Use {g} as the import
      // the next time through the loop.
      f = g;
    }
  }

})();

(function ConstructedTest() {
  print("ConstructedTest");

  var memory = undefined, table = undefined;
  for (let i = 0; i < 4; i++) {
    print("  iteration " + i);

    let m = new WebAssembly.Memory({initial: 1});
    let t = new WebAssembly.Table({element: "anyfunc", initial: 1});
    m.old = memory;
    t.old = table;

    memory = m;
    table = t;
    testProperties(memory);
    testProperties(table);
  }
})();
