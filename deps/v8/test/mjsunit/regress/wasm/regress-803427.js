// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
let module = new WebAssembly.Module(builder.toBuffer());
var worker = new Worker('onmessage = function() {};');
worker.postMessage(module)
