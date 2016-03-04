// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

try {
  var data = bytes(
      0,       kAstStmt,  // signature
      3,       0,         // local int32 count
      4,       0,         // local int64 count
      5,       0,         // local float32 count
      6,       0,         // local float64 count
      kExprNop            // body
  );

  _WASMEXP_.verifyFunction(data);
  print("ok");
} catch (e) {
  assertTrue(false);
}


var threw = false;
try {
  var data = bytes(
      0,       kAstI32,   // signature
      2,       0,         // local int32 count
      3,       0,         // local int64 count
      4,       0,         // local float32 count
      5,       0,         // local float64 count
      kExprBlock, 2, kExprNop, kExprNop  // body
  );

  _WASMEXP_.verifyFunction(data);
  print("not ok");
} catch (e) {
  print("ok: " + e);
  threw = true;
}

assertTrue(threw);
