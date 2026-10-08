// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const f2 = new WebAssemblyFunction(
    { parameters: ["i32"], results: ["i32"]}, (x) => 0);

const table = new WebAssembly.Table({
  element: "anyfunc",
  initial: 2,
});
table.set(1, f2);

assertEquals(0, f2(42));
