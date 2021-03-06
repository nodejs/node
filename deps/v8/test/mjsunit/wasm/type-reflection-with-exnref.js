// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-type-reflection --experimental-wasm-eh

load('test/mjsunit/wasm/wasm-module-builder.js');

(function TestGlobalType() {
  let global = new WebAssembly.Global({value: "exnref", mutable: true});
  let type = WebAssembly.Global.type(global);
  assertEquals("exnref", type.value);
  assertEquals(true, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);

  global = new WebAssembly.Global({value: "exnref"});
  type = WebAssembly.Global.type(global);
  assertEquals("exnref", type.value);
  assertEquals(false, type.mutable);
  assertEquals(2, Object.getOwnPropertyNames(type).length);
})();
