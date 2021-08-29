// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const memory = new WebAssembly.Memory({initial: 1});

let builder = new WasmModuleBuilder();
builder.addImportedMemory("imports", "mem");
builder.addFunction("fill", kSig_v_iii)
       .addBody([kExprLocalGet, 0, // dst
                 kExprLocalGet, 1, // value
                 kExprLocalGet, 2, // size
                 kNumericPrefix, kExprMemoryFill, 0]).exportAs("fill");
let instance = builder.instantiate({imports: {mem: memory}});
memory.grow(1);
assertTraps(
    kTrapMemOutOfBounds,
    () => instance.exports.fill(kPageSize + 1, 123, kPageSize));
