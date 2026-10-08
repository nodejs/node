// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation --log-code --vtune-prof-annotate-wasm

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("f", kSig_i_v).addBody([kExprI32Const, 42]).exportAs("f");
let url = "non_existent_file";
let url_bytes = [];
for (let i = 0; i < url.length; i++) url_bytes.push(url.charCodeAt(i));
builder.addCustomSection("sourceMappingURL", [url_bytes.length, ...url_bytes]);

let module = new WebAssembly.Module(builder.toBuffer());
let instance = new WebAssembly.Instance(module);

instance.exports.f();
