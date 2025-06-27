// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// We need a module with one valid function.
const builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_v_v).addBody([]);

const buffer = builder.toBuffer();
assertPromiseResult(
    WebAssembly.compile(buffer), _ => Realm.createAllowCrossRealmAccess());
