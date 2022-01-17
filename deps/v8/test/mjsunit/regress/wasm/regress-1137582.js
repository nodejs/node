// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const results = new Array(9).fill(kWasmI32);
builder.addFunction('foo', makeSig([], results)).addBody([kExprUnreachable]);
builder.instantiate();
