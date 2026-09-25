// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let $sig0 = builder.addType(kSig_v_v);

let $imp_exact = builder.addImport("", "exact", $sig0, kExternalExactFunction);
let $imp_inexact = builder.addImport("", "inexact", $sig0);
// Compile-time imports may also be exact.
builder.addImport("wasm:js-string", "test", kSig_i_r, kExternalExactFunction);

let $glob0 = builder.addGlobal(wasmRefType($sig0).exact(), true, false,
                               [kExprRefFunc, $imp_exact]);
builder.addFunction("set_exact", $sig0).addBody([
  kExprRefFunc, $imp_exact,
  kExprGlobalSet, $glob0.index,
]);

let imports = {"": {
  "exact": () => {},
  "inexact": () => {},
}};
let builtins = {builtins: ["js-string"]}
builder.instantiate(imports, builtins);  // Works.

builder.addGlobal(wasmRefType($sig0).exact(), true, false,
                  [kExprRefFunc, $imp_inexact]);

assertThrows(() => builder.instantiate(imports, builtins),
             WebAssembly.CompileError);

// Can't test a second reason to fail validation in the same builder.
let builder2 = new WasmModuleBuilder();
$sig0 = builder2.addType(kSig_v_v);
$imp_inexact = builder2.addImport("", "inexact", $sig0);
$glob0 = builder2.addGlobal(wasmRefType($sig0).exact(), true, false,
                            [kExprRefFunc, $imp_exact]);
builder2.addFunction("set_inexact", $sig0).addBody([
  kExprRefFunc, $imp_inexact,
  kExprGlobalSet, $glob0.index,
]);
assertThrows(() => builder2.instantiate(imports), WebAssembly.CompileError);
