// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --no-liftoff --nowasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var b = new WasmModuleBuilder();
let $bad = b.addFunction("bad", kSig_v_v).addBody([kExprLocalGet, 42]);
b.addFunction("caller", kSig_v_v).addBody([kExprCallFunction, $bad.index]);

assertPromiseResult(
    WebAssembly.instantiate(b.toBuffer()), assertUnreachable,
    e => assertException(
        e, WebAssembly.CompileError, /invalid local index: 42/));
