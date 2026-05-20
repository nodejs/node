// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addMemory(1, undefined);
builder.addFunction('load', kSig_i_i)
    .addBody([
        kExprLocalGet, 0,
    kExprI32LoadMem, 0, 100])
    .exportFunc();

const load = builder.instantiate().exports.load;
%WasmTierUpFunction(load);
// 100 is added as part of the load instruction above
// Last valid address (64k - 100 - 4)
assertEquals(0, load(0x10000 - 100 - 4));
// First invalid address (64k - 100)
assertTraps(kTrapMemOutOfBounds, _ => { load(0x10000 - 100);});
