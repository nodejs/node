// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

var module = (function () {
  var kFuncWithBody = 9;
  var kFuncImported = 7;
  var kBodySize1 = 1;
  var kMainOffset = 6 + kFuncWithBody + kBodySize1 + 1;

  var ffi = new Object();
  ffi.add = (function(a, b) { return a + b | 0; });

  return _WASMEXP_.instantiateModule(bytes(
    // -- signatures
    kDeclSignatures, 1,
    0, kAstStmt, // void -> void
    // -- function #0 (unreachable)
    kDeclFunctions, 1,
    kDeclFunctionName | kDeclFunctionExport,
    0, 0,                      // signature offset
    kMainOffset, 0, 0, 0,      // name offset
    kBodySize1, 0,             // body size
    kExprUnreachable,
    kDeclEnd,
    'm', 'a', 'i', 'n', 0      // name
  ), ffi);
})();

// Check the module exists.
assertFalse(module === undefined);
assertFalse(module === null);
assertFalse(module === 0);
assertEquals("object", typeof module);
assertEquals("function", typeof module.main);

var exception = "";
try {
    assertEquals(0, module.main());
} catch(e) {
    print("correctly caught: " + e);
    exception = e;
}
assertEquals("unreachable", exception);
