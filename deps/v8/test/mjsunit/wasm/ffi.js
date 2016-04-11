// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

function testCallFFI(func, check) {
  var kBodySize = 6;
  var kNameFunOffset = 24 + kBodySize + 1;
  var kNameMainOffset = kNameFunOffset + 4;

  var ffi = new Object();
  ffi.fun = func;

  var data = bytes(
    // signatures
    kDeclSignatures, 1,
    2, kAstI32, kAstF64, kAstF64, // (f64,f64) -> int
    // -- foreign function
    kDeclFunctions, 2,
    kDeclFunctionName | kDeclFunctionImport,
    0, 0,
    kNameFunOffset, 0, 0, 0,    // name offset
    // -- main function
    kDeclFunctionName | kDeclFunctionExport,
    0, 0,
    kNameMainOffset, 0, 0, 0,   // name offset
    kBodySize, 0,
    // main body
    kExprCallFunction, 0,       // --
    kExprGetLocal, 0,           // --
    kExprGetLocal, 1,           // --
    // names
    kDeclEnd,
    'f', 'u', 'n', 0,           //  --
    'm', 'a', 'i', 'n', 0       //  --
  );

  var module = _WASMEXP_.instantiateModule(data, ffi);

  assertEquals("function", typeof module.main);

  for (var i = 0; i < 100000; i += 10003) {
    var a = 22.5 + i, b = 10.5 + i;
    var r = module.main(a, b);
    check(r, a, b);
  }
}

var global = (function() { return this; })();
var params = [-99, -99, -99, -99];
var was_called = false;
var length = -1;

function FOREIGN_SUB(a, b) {
  print("FOREIGN_SUB(" + a + ", " + b + ")");
  was_called = true;
  params[0] = this;
  params[1] = a;
  params[2] = b;
  return (a - b) | 0;
}

function check_FOREIGN_SUB(r, a, b) {
    assertEquals(a - b | 0, r);
    assertTrue(was_called);
//    assertEquals(global, params[0]);  // sloppy mode
    assertEquals(a, params[1]);
    assertEquals(b, params[2]);
    was_called = false;
}

testCallFFI(FOREIGN_SUB, check_FOREIGN_SUB);


function FOREIGN_ABCD(a, b, c, d) {
  print("FOREIGN_ABCD(" + a + ", " + b + ", " + c + ", " + d + ")");
  was_called = true;
  params[0] = this;
  params[1] = a;
  params[2] = b;
  params[3] = c;
  params[4] = d;
  return (a * b * 6) | 0;
}

function check_FOREIGN_ABCD(r, a, b) {
    assertEquals((a * b * 6) | 0, r);
    assertTrue(was_called);
//    assertEquals(global, params[0]);  // sloppy mode.
    assertEquals(a, params[1]);
    assertEquals(b, params[2]);
    assertEquals(undefined, params[3]);
    assertEquals(undefined, params[4]);
    was_called = false;
}

testCallFFI(FOREIGN_ABCD, check_FOREIGN_ABCD);

function FOREIGN_ARGUMENTS0() {
  print("FOREIGN_ARGUMENTS0");
  was_called = true;
  length = arguments.length;
  for (var i = 0; i < arguments.length; i++) {
    params[i] = arguments[i];
  }
  return (arguments[0] * arguments[1] * 7) | 0;
}

function FOREIGN_ARGUMENTS1(a) {
  print("FOREIGN_ARGUMENTS1", a);
  was_called = true;
  length = arguments.length;
  for (var i = 0; i < arguments.length; i++) {
    params[i] = arguments[i];
  }
  return (arguments[0] * arguments[1] * 7) | 0;
}

function FOREIGN_ARGUMENTS2(a, b) {
  print("FOREIGN_ARGUMENTS2", a, b);
  was_called = true;
  length = arguments.length;
  for (var i = 0; i < arguments.length; i++) {
    params[i] = arguments[i];
  }
  return (a * b * 7) | 0;
}

function FOREIGN_ARGUMENTS3(a, b, c) {
  print("FOREIGN_ARGUMENTS3", a, b, c);
  was_called = true;
  length = arguments.length;
  for (var i = 0; i < arguments.length; i++) {
    params[i] = arguments[i];
  }
  return (a * b * 7) | 0;
}

function FOREIGN_ARGUMENTS4(a, b, c, d) {
  print("FOREIGN_ARGUMENTS4", a, b, c, d);
  was_called = true;
  length = arguments.length;
  for (var i = 0; i < arguments.length; i++) {
    params[i] = arguments[i];
  }
  return (a * b * 7) | 0;
}

function check_FOREIGN_ARGUMENTS(r, a, b) {
  assertEquals((a * b * 7) | 0, r);
  assertTrue(was_called);
  assertEquals(2, length);
  assertEquals(a, params[0]);
  assertEquals(b, params[1]);
  was_called = false;
}

// Check a bunch of uses of the arguments object.
testCallFFI(FOREIGN_ARGUMENTS0, check_FOREIGN_ARGUMENTS);
testCallFFI(FOREIGN_ARGUMENTS1, check_FOREIGN_ARGUMENTS);
testCallFFI(FOREIGN_ARGUMENTS2, check_FOREIGN_ARGUMENTS);
testCallFFI(FOREIGN_ARGUMENTS3, check_FOREIGN_ARGUMENTS);
testCallFFI(FOREIGN_ARGUMENTS4, check_FOREIGN_ARGUMENTS);

function returnValue(val) {
  return function(a, b) {
    print("RETURN_VALUE ", val);
    return val;
  }
}


function checkReturn(expected) {
  return function(r, a, b) { assertEquals(expected, r); }
}

// Check that returning weird values doesn't crash
testCallFFI(returnValue(undefined), checkReturn(0));
testCallFFI(returnValue(null), checkReturn(0));
testCallFFI(returnValue("0"), checkReturn(0));
testCallFFI(returnValue("-77"), checkReturn(-77));

var objWithValueOf = {valueOf: function() { return 198; }}

testCallFFI(returnValue(objWithValueOf), checkReturn(198));


function testCallBinopVoid(type, func, check) {
  var kBodySize = 10;
  var kNameFunOffset = 28 + kBodySize + 1;
  var kNameMainOffset = kNameFunOffset + 4;

  var ffi = new Object();

  var passed_length = -1;
  var passed_a = -1;
  var passed_b = -1;
  var args_a = -1;
  var args_b = -1;

  ffi.fun = function(a, b) {
    passed_length = arguments.length;
    passed_a = a;
    passed_b = b;
    args_a = arguments[0];
    args_b = arguments[1];
  }

  var data = bytes(
    // -- signatures
    kDeclSignatures, 2,
    2, kAstStmt, type, type,    // (type,type)->void
    2, kAstI32, type, type,     // (type,type)->int
    // -- foreign function
    kDeclFunctions, 2,
    kDeclFunctionName | kDeclFunctionImport,
    0, 0,                       // signature index
    kNameFunOffset, 0, 0, 0,    // name offset
    // -- main function
    kDeclFunctionName | kDeclFunctionExport,
    1, 0,                       // signature index
    kNameMainOffset, 0, 0, 0,   // name offset
    kBodySize, 0,               // body size
    // main body
    kExprBlock, 2,              // --
    kExprCallFunction, 0,       // --
    kExprGetLocal, 0,           // --
    kExprGetLocal, 1,           // --
    kExprI8Const, 99,           // --
    // names
    kDeclEnd,
    'f', 'u', 'n', 0,           // --
    'm', 'a', 'i', 'n', 0       // --
  );

  var module = _WASMEXP_.instantiateModule(data, ffi);

  assertEquals("function", typeof module.main);

  print("testCallBinopVoid", type);

  for (var i = 0; i < 100000; i += 10003.1) {
    var a = 22.5 + i, b = 10.5 + i;
    var r = module.main(a, b);
    assertEquals(99, r);
    assertEquals(2, passed_length);
    var expected_a, expected_b;
    switch (type) {
      case kAstI32: {
        expected_a = a | 0;
        expected_b = b | 0;
        break;
      }
      case kAstF32: {
        expected_a = Math.fround(a);
        expected_b = Math.fround(b);
        break;
      }
      case kAstF64: {
        expected_a = a;
        expected_b = b;
        break;
      }
    }

    assertEquals(expected_a, args_a);
    assertEquals(expected_b, args_b);
    assertEquals(expected_a, passed_a);
    assertEquals(expected_b, passed_b);
  }
}


testCallBinopVoid(kAstI32);
// TODO testCallBinopVoid(kAstI64);
testCallBinopVoid(kAstF32);
testCallBinopVoid(kAstF64);



function testCallPrint() {
  var kBodySize = 10;
  var kNamePrintOffset = 10 + 7 + 7 + 9 + kBodySize + 1;
  var kNameMainOffset = kNamePrintOffset + 6;

  var ffi = new Object();
  ffi.print = print;

  var data = bytes(
    // -- signatures
    kDeclSignatures, 2,
    1, kAstStmt, kAstI32,       // i32->void
    1, kAstStmt, kAstF64,       // f64->int
    kDeclFunctions, 3,
    // -- import print i32
    kDeclFunctionName | kDeclFunctionImport,
    0, 0,                       // signature index
    kNamePrintOffset, 0, 0, 0,  // name offset
    // -- import print f64
    kDeclFunctionName | kDeclFunctionImport,
    1, 0,                       // signature index
    kNamePrintOffset, 0, 0, 0,  // name offset
    // -- decl main
    kDeclFunctionName | kDeclFunctionExport,
    1, 0,                       // signature index
    kNameMainOffset, 0, 0, 0,   // name offset
    kBodySize, 0,               // body size
    // main body
    kExprBlock, 2,              // --
    kExprCallFunction, 0,       // --
    kExprI8Const, 97,           // --
    kExprCallFunction, 1,       // --
    kExprGetLocal, 0,           // --
    // names
    kDeclEnd,
    'p', 'r', 'i', 'n', 't', 0, // --
    'm', 'a', 'i', 'n', 0       // --
  );

  var module = _WASMEXP_.instantiateModule(data, ffi);

  assertEquals("function", typeof module.main);

  for (var i = -9; i < 900; i += 6.125) {
      module.main(i);
  }
}

testCallPrint();
testCallPrint();
