// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-rab-integration

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let module = (() => {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, undefined);
  builder.exportMemoryAs("memory");
  return builder.toModule();
})();

(function TestNoMaxErrorViaAPI() {
  print("TestNoMaxErrorViaAPI...");
  let memory = new WebAssembly.Memory({initial: 1});
  assertThrows(() => memory.toResizableBuffer(), TypeError);
})();

(function TestNoMaxErrorViaBytecode() {
  print("TestNoMaxErrorViaBytecode...");
  let instance = new WebAssembly.Instance(module);
  let memory = instance.exports.memory;
  assertThrows(() => memory.toResizableBuffer(), TypeError);
})();

(function TestResizeNonPageMultiple() {
  print("TestResizeNonPageMultiple...");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 2});
  let buf = memory.toResizableBuffer();
  assertThrows(() => buf.resize(kPageSize + 1), RangeError);
})();

(function TestResizeNoShrink() {
  print("TestResizeNoShrink...");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 2});
  let buf = memory.toResizableBuffer();
  assertThrows(() => buf.resize(kPageSize - 1), RangeError);
})();
