// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection --experimental-wasm-anyref

load('test/mjsunit/wasm/wasm-module-builder.js');

(function TestTableType() {
  let table = new WebAssembly.Table({initial: 1, element: "anyref"});
  let type = WebAssembly.Table.type(table);
  assertEquals(1, type.minimum);
  assertEquals("anyref", type.element);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  table = new WebAssembly.Table({initial: 2, maximum: 15, element: "anyref"});
  type = WebAssembly.Table.type(table);
  assertEquals(2, type.minimum);
  assertEquals(15, type.maximum);
  assertEquals("anyref", type.element);
  assertEquals(3, Object.getOwnPropertyNames(type).length);
})();

(function TestGlobalType() {
  let global = new WebAssembly.Global({value: "anyref", mutable: true});
  let type = WebAssembly.Global.type(global);
  assertEquals("anyref", type.value);
  assertEquals(true, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  global = new WebAssembly.Global({value: "anyref"});
  type = WebAssembly.Global.type(global);
  assertEquals("anyref", type.value);
  assertEquals(false, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);
})();
