// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_d_v)
    .addLocals(kWasmExternRef, 16000)
    .addBody([kExprUnreachable]);
builder.toModule();
