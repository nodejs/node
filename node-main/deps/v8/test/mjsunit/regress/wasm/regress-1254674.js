// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --print-wasm-code

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addImport('Math', 'sqrt', kSig_d_d);
builder.instantiate({Math: Math});
