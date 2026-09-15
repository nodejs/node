// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

builder.addImport('wasm:js-string', 'test', kSig_i_r);
builder.addImport('m', 'func', kSig_d_d);

let sin = {m: {func: Math.sin}};
let cos = {m: {func: Math.cos}};
let builtins = {builtins: ["js-string"]};

builder.instantiate(sin, builtins);  // All good.
builder.instantiate(cos, builtins);  // Mark imports as "generic".
builder.instantiate(sin, builtins);  // Bug: compile-time imports fail.
