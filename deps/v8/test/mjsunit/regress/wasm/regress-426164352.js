// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-wasm-code-gc
// Flags: --experimental-wasm-growable-stacks --enable-testing-opcode-in-wasm
// Flags: --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let sig = builder.addType(kSig_r_r);
builder.addFunction("func0", sig).addBody([kExprUnreachable]).exportFunc();
builder.addFunction('main', kSig_v_v).exportFunc().addBody(
    [kExprNopForTestingUnsupportedInLiftoff]);
let instance = builder.instantiate();
%WasmEnterDebugging();
instance.exports.main();
