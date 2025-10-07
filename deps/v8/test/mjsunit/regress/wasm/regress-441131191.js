// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
var module_bytes = builder.toBuffer();
var module = new WebAssembly.Module(module_bytes);
var serialized_bytes = d8.wasm.serializeModule(module);
serialized_bytes.transfer();
assertThrows(() => d8.wasm.deserializeModule(serialized_bytes, module_bytes),
             Error,
             /First argument is detached/);

serialized_bytes = d8.wasm.serializeModule(module);
module_bytes.buffer.transfer();
assertThrows(() => d8.wasm.deserializeModule(serialized_bytes, module_bytes),
             Error,
             /Second argument's buffer is detached/);
