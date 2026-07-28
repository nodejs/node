// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --wasm-num-compilation-tasks=0

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const buffer = builder.toBuffer();

assertPromiseResult(WebAssembly.compileStreaming(Promise.resolve(buffer)));
