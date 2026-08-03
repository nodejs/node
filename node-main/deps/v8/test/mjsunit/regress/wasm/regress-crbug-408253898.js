// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let module_builder = new WasmModuleBuilder();
module_builder.addImportedGlobal("foo", "oh", kWasmExternRef);
module_builder.addImportedGlobal("bar", "no", kWasmExternRef);
let wasm_imports = WebAssembly.Module.imports(module_builder.toModule({
  importedStringConstants: "foo"
}));

assertEquals(wasm_imports.length, 1);
assertTrue(%HasFastPackedElements(wasm_imports));
assertTrue(%HeapObjectVerify(wasm_imports));

wasm_imports.fill(0);

assertEquals(wasm_imports.length, 1);
assertTrue(%HasSmiElements(wasm_imports));
assertTrue(%HasFastPackedElements(wasm_imports));
assertTrue(%HeapObjectVerify(wasm_imports));
