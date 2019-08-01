// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_v).addBody([]);
let rethrow = e => setTimeout(_ => {throw e}, 0);
WebAssembly.compile(builder.toBuffer()).catch(rethrow);
