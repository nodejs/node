// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new WebAssembly.Memory({initial: 1});

let builder = new WasmModuleBuilder();
builder.addImportedMemory("imports", "mem", 1);
builder.addFunction("copy", kSig_v_iii)
       .addBody([kExprLocalGet, 0, // dst
                 kExprLocalGet, 1, // src
                 kExprLocalGet, 2, // size
                 kNumericPrefix, kExprMemoryCopy, 0, 0]).exportAs("copy");
let instance = builder.instantiate({imports: {mem: memory}});
memory.grow(1);
instance.exports.copy(0, kPageSize, 11);
