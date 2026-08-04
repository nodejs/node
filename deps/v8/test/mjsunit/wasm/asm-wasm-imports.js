// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

var stdlib = this;

function assertValidAsm(func) {
  assertTrue(%IsAsmWasmCode(func), "must be valid asm code");
}

function assertWasm(expected, func, ffi) {
  print("Testing " + func.name + "...");
  assertEquals(
      expected, func(stdlib, ffi, new ArrayBuffer(1024)).caller());
  assertValidAsm(func);
}


function TestForeignFunctions() {
  function AsmModule(stdlib, foreign, buffer) {
    "use asm";

    var setVal = foreign.setVal;
    var getVal = foreign.getVal;

    function caller(initial_value, new_value) {
      initial_value = initial_value|0;
      new_value = new_value|0;
      if ((getVal()|0) == (initial_value|0)) {
        setVal(new_value|0);
        return getVal()|0;
      }
      return 0;
    }

    return {caller:caller};
  }

  function ffi(initial_val) {
    var val = initial_val;

    function getVal() {
      return val;
    }

    function setVal(new_val) {
      val = new_val;
    }

    return {getVal:getVal, setVal:setVal};
  }

  var foreign = new ffi(23);

  var module = AsmModule({Math: Math}, foreign, null);
  assertValidAsm(AsmModule);

  assertEquals(103, module.caller(23, 103));
}

print("TestForeignFunctions...");
TestForeignFunctions();


function TestForeignFunctionMultipleUse() {
  function AsmModule(stdlib, foreign, buffer) {
    "use asm";

    var getVal = foreign.getVal;

    function caller(int_val, double_val) {
      int_val = int_val|0;
      double_val = +double_val;
      if ((getVal()|0) == (int_val|0)) {
        if ((+getVal()) == (+double_val)) {
          return 89;
        }
      }
      return 0;
    }

    return {caller:caller};
  }

  function ffi() {
    function getVal() {
      return 83.25;
    }

    return {getVal:getVal};
  }

  var foreign = new ffi();

  var module_decl = eval('(' + AsmModule.toString() + ')');
  var module = module_decl(stdlib, foreign, null);
  assertValidAsm(module_decl);

  assertEquals(89, module.caller(83, 83.25));
}

print("TestForeignFunctionMultipleUse...");
TestForeignFunctionMultipleUse();

function TestForeignVariables() {
  function AsmModule(stdlib, foreign, buffer) {
    "use asm";

    var i1 = foreign.foo | 0;
    var f1 = +foreign.bar;
    var i2 = foreign.baz | 0;
    var f2 = +foreign.baz;

    function geti1() {
      return i1|0;
    }

    function getf1() {
      return +f1;
    }

    function geti2() {
      return i2|0;
    }

    function getf2() {
      return +f2;
    }

    return {geti1:geti1, getf1:getf1, geti2:geti2, getf2:getf2};
  }

  function TestCase(env, i1, f1, i2, f2) {
    print("Testing foreign variables...");
    var module_decl = eval('(' + AsmModule.toString() + ')');
    var module = module_decl(stdlib, env);
    assertValidAsm(module_decl);
    assertEquals(i1, module.geti1());
    assertEquals(f1, module.getf1());
    assertEquals(i2, module.geti2());
    assertEquals(f2, module.getf2());
  }

  // Check normal operation.
  TestCase({foo: 123, bar: 234.5, baz: 345.7}, 123, 234.5, 345, 345.7);
  // Check partial operation.
  TestCase({baz: 345.7}, 0, NaN, 345, 345.7);
  // Check that undefined values are converted to proper defaults.
  TestCase({qux: 999}, 0, NaN, 0, NaN);
  // Check that true values are converted properly.
  TestCase({foo: true, bar: true, baz: true}, 1, 1.0, 1, 1.0);
  // Check that false values are converted properly.
  TestCase({foo: false, bar: false, baz: false}, 0, 0, 0, 0);
  // Check that null values are converted properly.
  TestCase({foo: null, bar: null, baz: null}, 0, 0, 0, 0);
  // Check that string values are converted properly.
  TestCase({foo: 'hi', bar: 'there', baz: 'dude'}, 0, NaN, 0, NaN);
  TestCase({foo: '0xff', bar: '234', baz: '456.1'}, 255, 234, 456, 456.1);
  // Check that function values are converted properly.
  TestCase({foo: TestCase, bar: TestCase, qux: TestCase}, 0, NaN, 0, NaN);
}

print("TestForeignVariables...");
TestForeignVariables();


function TestGlobalBlock(stdlib, foreign, buffer) {
  "use asm";

  var x = foreign.x | 0, y = foreign.y | 0;

  function test() {
    return (x + y) | 0;
  }

  return {caller: test};
}

assertWasm(15, TestGlobalBlock, { x: 4, y: 11 });
