// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --invoke-weak-callbacks --omit-quit --wasm-async-compilation --expose-wasm --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 42]);
const buffer = builder.toBuffer();
// Start async compilation, but don't wait for it to finish.
const module = WebAssembly.compile(buffer);

// This create the collator.
'퓛'.localeCompare();
