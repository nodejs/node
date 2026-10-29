// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction("no_body", kSig_v_v);
const buffer = builder.toBuffer();
let options = { importedStringConstants: "foo" };
assertThrowsAsync(WebAssembly.compile(buffer, options),
                  WebAssembly.CompileError,
                  /function body must end with "end" opcode/);
