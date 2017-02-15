// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

try {
  var data = bytes(
      kWasmFunctionTypeForm, 0, kAstStmt,  // signature
      kDeclNoLocals,                       // --
      kExprNop                             // body
  );

  Wasm.verifyFunction(data);
  print("ok");
} catch (e) {
  assertTrue(false);
}


var threw = false;
try {
  var data = bytes(
      kWasmFunctionTypeForm, 0, 1, kAstI32,     // signature
      kDeclNoLocals,                            // --
      kExprBlock, kAstStmt, kExprNop, kExprNop, kExprEnd  // body
  );

  Wasm.verifyFunction(data);
  print("not ok");
} catch (e) {
  print("ok: " + e);
  threw = true;
}

assertTrue(threw);
