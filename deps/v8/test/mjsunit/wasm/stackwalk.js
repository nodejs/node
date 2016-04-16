// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");

function makeFFI(func) {
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

  return module.main;
}


function makeReentrantFFI(func) {
  var main = makeFFI(reenter);

  function reenter(a, b) {
    print(" reenter " + a);
    if (a > 0) main(a - 1, b);
    else func();
  }
  return main;
}


function runTest(builder) {
  // ---- THROWING TEST -----------------------------------------------

  function throwadd(a, b) {
    print("-- trying throw --");
    throw a + b;
  }

  function throwa(a) {
    print("-- trying throw --");
    throw a;
  }

  function throwstr() {
    print("-- trying throw --");
    throw "string";
  }

  assertThrows(builder(throwadd));
  assertThrows(builder(throwa));
  assertThrows(builder(throwstr));

  try {
    builder(throwadd)(7.8, 9.9);
  } catch(e) {
    print(e);
  }

  try {
    builder(throwa)(11.8, 9.3);
  } catch(e) {
    print(e);
  }


  try {
    builder(throwstr)(3, 5);
  } catch(e) {
    print(e);
  }


  // ---- DEOPT TEST -----------------------------------------------

  function deopt() {
    print("-- trying deopt --");
    %DeoptimizeFunction(deopter);
  }

  var deopter = builder(deopt);

  deopter(5, 5);
  for (var i = 0; i < 9; i++) {
    deopter(6, 6);
  }


  // ---- GC TEST -----------------------------------------------
  function dogc(a, b) {
    print("-- trying gc --");
    gc();
    gc();
  }


  var gcer = builder(dogc);
  gcer(7, 7);

  for (var i = 0; i < 9; i++) {
    gcer(8, 8);
  }
}

runTest(makeReentrantFFI);
runTest(makeFFI);
