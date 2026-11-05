// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --correctness-fuzzer-suppressions

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
// Valid as a normal import, but invalid when 'js-string' builtins are enabled.
builder.addImport('wasm:js-string', 'cast', kSig_i_i);
const wire_bytes = builder.toBuffer();

const module = new WebAssembly.Module(wire_bytes);
const serialized = d8.wasm.serializeModule(module);
assertThrows(
    () => d8.wasm.deserializeModule(
        serialized, wire_bytes, {builtins: ['js-string']}),
    WebAssembly.CompileError);
