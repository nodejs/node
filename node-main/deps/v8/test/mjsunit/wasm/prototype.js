// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let emptyModuleBinary = new WasmModuleBuilder().toBuffer();

(function ModulePrototype() {
  class _Module extends WebAssembly.Module {}
  let module = new _Module(emptyModuleBinary);
  assertInstanceof(module, _Module);
  assertInstanceof(module, WebAssembly.Module);
})();

(function InstancePrototype() {
  class _Instance extends WebAssembly.Instance {}
  let instance = new _Instance(new WebAssembly.Module(emptyModuleBinary));
  assertInstanceof(instance, _Instance);
  assertInstanceof(instance, WebAssembly.Instance);
})();

(function TablePrototype() {
  class _Table extends WebAssembly.Table {}
  let table = new _Table({initial: 0, element: "anyfunc"});
  assertInstanceof(table, _Table);
  assertInstanceof(table, WebAssembly.Table);
})();

(function MemoryPrototype() {
  class _Memory extends WebAssembly.Memory {}
  let memory = new _Memory({initial: 0, maximum: 1});
  assertInstanceof(memory, _Memory);
  assertInstanceof(memory, WebAssembly.Memory);
})();

(function GlobalPrototype() {
  class _Global extends WebAssembly.Global {}
  let global = new _Global({value: 'i32', mutable: false}, 0);
  assertInstanceof(global, _Global);
  assertInstanceof(global, WebAssembly.Global);
})();
