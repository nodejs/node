// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

class C6 {}

const o11 = {
    "parameters": [],
    "results": [],
};
const v12 = new WebAssemblyFunction(o11, C6);
const o17 = {
    "element": "anyfunc",
    "initial": 1,
};
const v18 = new WebAssembly.Table(o17);
v18.set(0, v12);
