// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestAsyncCompileMultipleCodeSections() {
  let binary = new Binary();
  binary.emit_header();
  binary.push(kTypeSectionCode, 4, 1, kWasmFunctionTypeForm, 0, 0);
  binary.push(kFunctionSectionCode, 2, 1, 0);
  binary.push(kCodeSectionCode, 6, 1, 4, 0, kExprGetLocal, 0, kExprEnd);
  binary.push(kCodeSectionCode, 6, 1, 4, 0, kExprGetLocal, 0, kExprEnd);
  let buffer = Uint8Array.from(binary).buffer;
  assertPromiseResult(WebAssembly.compile(buffer), assertUnreachable,
                      e => assertInstanceof(e, WebAssembly.CompileError));
})();
