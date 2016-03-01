// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

function runSelect2(module, which, a, b) {
  assertEquals(which == 0 ? a : b, module.select(a, b));
}

function testSelect2(type) {
  var kBodySize = 2;
  var kNameOffset = 21 + kBodySize + 1;

  for (var which = 0; which < 2; which++) {
    print("type = " + type + ", which = " + which);

    var data = bytes(
      // -- memory
      kDeclMemory,
      12, 12, 1,                  // memory
      // -- signatures
      kDeclSignatures, 1,
      2, type, type, type,        // signature: (t,t)->t
      // -- select
      kDeclFunctions, 1,
      kDeclFunctionName | kDeclFunctionExport,
      0, 0,
      kNameOffset, 0, 0, 0,       // name offset
      kBodySize, 0,               // body size
      kExprGetLocal, which,       // --
      kDeclEnd,
      's','e','l','e','c','t',0   // name
    );

    var module = _WASMEXP_.instantiateModule(data);

    assertEquals("function", typeof module.select);
    runSelect2(module, which, 99, 97);
    runSelect2(module, which, -99, -97);

    if (type != kAstF32) {
      runSelect2(module, which, 0x80000000 | 0, 0x7fffffff | 0);
      runSelect2(module, which, 0x80000001 | 0, 0x7ffffffe | 0);
      runSelect2(module, which, 0xffffffff | 0, 0xfffffffe | 0);
      runSelect2(module, which, -2147483647, 2147483646);
      runSelect2(module, which, -2147483646, 2147483645);
      runSelect2(module, which, -2147483648, 2147483647);
    }

    if (type != kAstI32 && type != kAstI64) {
      runSelect2(module, which, -1.25, 5.25);
      runSelect2(module, which, Infinity, -Infinity);
    }
  }
}


testSelect2(kAstI32);
testSelect2(kAstF32);
testSelect2(kAstF64);


function runSelect10(module, which, a, b) {
  var x = -1;

  var result = [
    module.select(a, b, x, x, x, x, x, x, x, x),
    module.select(x, a, b, x, x, x, x, x, x, x),
    module.select(x, x, a, b, x, x, x, x, x, x),
    module.select(x, x, x, a, b, x, x, x, x, x),
    module.select(x, x, x, x, a, b, x, x, x, x),
    module.select(x, x, x, x, x, a, b, x, x, x),
    module.select(x, x, x, x, x, x, a, b, x, x),
    module.select(x, x, x, x, x, x, x, a, b, x),
    module.select(x, x, x, x, x, x, x, x, a, b),
    module.select(x, x, x, x, x, x, x, x, x, a)
  ];

  for (var i = 0; i < 10; i++) {
     if (which == i) assertEquals(a, result[i]);
     else if (which == i+1) assertEquals(b, result[i]);
     else assertEquals(x, result[i]);
  }
}

function testSelect10(type) {
  var kBodySize = 2;
  var kNameOffset = 29 + kBodySize + 1;

  for (var which = 0; which < 10; which++) {
    print("type = " + type + ", which = " + which);

    var t = type;
    var data = bytes(
      kDeclMemory,
      12, 12, 1,                  // memory
      // signatures
      kDeclSignatures, 1,
      10, t,t,t,t,t,t,t,t,t,t,t,  // (tx10)->t
      // main function
      kDeclFunctions, 1,
      kDeclFunctionName | kDeclFunctionExport,
      0, 0,
      kNameOffset, 0, 0, 0,       // name offset
      kBodySize, 0,               // body size
      kExprGetLocal, which,       // --
      kDeclEnd,
      's','e','l','e','c','t',0   // name
    );

    var module = _WASMEXP_.instantiateModule(data);

    assertEquals("function", typeof module.select);
    runSelect10(module, which, 99, 97);
    runSelect10(module, which, -99, -97);

    if (type != kAstF32) {
      runSelect10(module, which, 0x80000000 | 0, 0x7fffffff | 0);
      runSelect10(module, which, 0x80000001 | 0, 0x7ffffffe | 0);
      runSelect10(module, which, 0xffffffff | 0, 0xfffffffe | 0);
      runSelect10(module, which, -2147483647, 2147483646);
      runSelect10(module, which, -2147483646, 2147483645);
      runSelect10(module, which, -2147483648, 2147483647);
    }

    if (type != kAstI32 && type != kAstI64) {
      runSelect10(module, which, -1.25, 5.25);
      runSelect10(module, which, Infinity, -Infinity);
    }
  }
}


testSelect10(kAstI32);
testSelect10(kAstF32);
testSelect10(kAstF64);
