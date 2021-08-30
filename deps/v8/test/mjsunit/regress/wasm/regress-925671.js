// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-tier-mask-for-testing=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction('f0', kSig_v_v).addBody([]);
builder.addFunction('f1', kSig_v_v).addBody([]);
builder.instantiate();
