// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
var instance = builder.instantiate();
instance[1] = undefined;
gc();
Object.getOwnPropertyNames(instance);
