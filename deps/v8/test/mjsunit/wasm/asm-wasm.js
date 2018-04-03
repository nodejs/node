// Copyright 2015 the V8 project authors. All rights reserved.
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

function EmptyTest(a, b, c) {
  "use asm";
  function caller() {
    empty();
    return 11;
  }
  function empty() {
  }
  return {caller: caller};
}

assertWasm(11, EmptyTest);

function VoidReturnTest(a, b, c) {
  "use asm";
  function caller() {
    empty();
    return 19;
  }
  function empty() {
    var x = 0;
    if (x) return;
  }
  return {caller: caller};
}

assertWasm(19, VoidReturnTest);

function IntTest(a, b, c) {
  "use asm";
  function sum(a, b) {
    a = a|0;
    b = b|0;
    var c = 0;
    var d = 3.0;
    var e = 0;
    e = ~~d;  // double conversion
    c = (b + 1)|0
    return (a + c + 1)|0;
  }

  function caller() {
    return sum(77,22) | 0;
  }

  return {caller: caller};
}

assertWasm(101,IntTest);


function Float64Test() {
  "use asm";
  function sum(a, b) {
    a = +a;
    b = +b;
    return +(a + b);
  }

  function caller() {
    var a = 0.0;
    var ret = 0;
    a = +sum(70.1,10.2);
    if (a == 80.3) {
      ret = 1|0;
    } else {
      ret = 0|0;
    }
    return ret|0;
  }

  return {caller: caller};
}

assertWasm(1, Float64Test);


function BadModule() {
  "use asm";
  function caller(a, b) {
    a = a|0;
    b = b+0;
    var c = 0;
    c = (b + 1)|0
    return (a + c + 1)|0;
  }

  function caller() {
    return call(1, 2)|0;
  }

  return {caller: caller};
}

assertFalse(%IsAsmWasmCode(BadModule));


function TestReturnInBlock() {
  "use asm";

  function caller() {
    if(1) {
      {
        {
          return 1;
        }
      }
    }
    return 0;
  }

  return {caller: caller};
}

assertWasm(1, TestReturnInBlock);


function TestAddSimple() {
  "use asm";

  function caller() {
    var x = 0;
    x = (x + 1)|0;
    return x|0;
  }

  return {caller: caller};
}

assertWasm(1, TestAddSimple);


function TestWhileSimple() {
  "use asm";

  function caller() {
    var x = 0;
    while((x|0) < 5) {
      x = (x + 1)|0;
    }
    return x|0;
  }

  return {caller: caller};
}

assertWasm(5, TestWhileSimple);


function TestWhileWithoutBraces() {
  "use asm";

  function caller() {
    var x = 0;
    while((x|0) <= 3)
      x = (x + 1)|0;
    return x|0;
  }

  return {caller: caller};
}

assertWasm(4, TestWhileWithoutBraces);


function TestReturnInWhile() {
  "use asm";

  function caller() {
    var x = 0;
    while((x|0) < 10) {
      x = (x + 6)|0;
      return x|0;
    }
    return x|0;
  }

  return {caller: caller};
}

assertWasm(6, TestReturnInWhile);


function TestReturnInWhileWithoutBraces() {
  "use asm";

  function caller() {
    var x = 0;
    while((x|0) < 5)
      return 7;
    return x|0;
  }

  return {caller: caller};
}

assertWasm(7, TestReturnInWhileWithoutBraces);


function TestBreakInIf() {
  "use asm";

  function caller() {
    label: {
      if(1) break label;
      return 11;
    }
    return 12;
  }

  return {caller: caller};
}

assertWasm(12, TestBreakInIf);

function TestBreakInIfInDoWhileFalse() {
  "use asm";

  function caller() {
    do {
      if(1) break;
      return 11;
    } while(0);
    return 12;
  }

  return {caller: caller};
}

assertWasm(12, TestBreakInIfInDoWhileFalse);

function TestBreakInElse() {
  "use asm";

  function caller() {
    do {
      if(0) ;
      else break;
      return 14;
    } while(0);
    return 15;
  }

  return {caller: caller};
}

assertWasm(15, TestBreakInElse);

function TestBreakInWhile() {
  "use asm";

  function caller() {
    while(1) {
      break;
    }
    return 8;
  }

  return {caller: caller};
}

assertWasm(8, TestBreakInWhile);


function TestBreakInIfInWhile() {
  "use asm";

  function caller() {
    while(1) {
      if (1) break;
      else break;
    }
    return 8;
  }

  return {caller: caller};
}

assertWasm(8, TestBreakInIfInWhile);

function TestBreakInNestedWhile() {
  "use asm";

  function caller() {
    var x = 1.0;
    var ret = 0;
    while(x < 1.5) {
      while(1)
        break;
      x = +(x + 0.25);
    }
    if (x == 1.5) {
      ret = 9;
    }
    return ret|0;
  }

  return {caller: caller};
}

assertWasm(9, TestBreakInNestedWhile);


function TestBreakInBlock() {
  "use asm";

  function caller() {
    var x = 0;
    abc: {
      x = 10;
      if ((x|0) == 10) {
        break abc;
      }
      x = 20;
    }
    return x|0;
  }

  return {caller: caller};
}

assertWasm(10, TestBreakInBlock);


function TestBreakInNamedWhile() {
  "use asm";

  function caller() {
    var x = 0;
    outer: while (1) {
      x = (x + 1)|0;
      while ((x|0) == 11) {
        break outer;
      }
    }
    return x|0;
  }

  return {caller: caller};
}

assertWasm(11, TestBreakInNamedWhile);


function TestContinue() {
  "use asm";

  function caller() {
    var x = 5;
    var ret = 0;
    while ((x|0) >= 0) {
      x = (x - 1)|0;
      if ((x|0) == 2) {
        continue;
      }
      ret = (ret - 1)|0;
    }
    return ret|0;
  }

  return {caller: caller};
}

assertWasm(-5, TestContinue);


function TestContinueInNamedWhile() {
  "use asm";

  function caller() {
    var x = 5;
    var y = 0;
    var ret = 0;
    outer: while ((x|0) > 0) {
      x = (x - 1)|0;
      y = 0;
      while ((y|0) < 5) {
        if ((x|0) == 3) {
          continue outer;
        }
        ret = (ret + 1)|0;
        y = (y + 1)|0;
      }
    }
    return ret|0;
  }

  return {caller: caller};
}

assertWasm(20, TestContinueInNamedWhile);


function TestContinueInDoWhileFalse() {
  "use asm";

  function caller() {
    do {
      continue;
    } while (0);
    return 47;
  }

  return {caller: caller};
}

assertWasm(47, TestContinueInDoWhileFalse);


function TestContinueInForLoop() {
  "use asm";

  function caller() {
    var i = 0;
    for (; (i|0) < 10; i = (i+1)|0) {
      continue;
    }
    return 4711;
  }

  return {caller: caller};
}

assertWasm(4711, TestContinueInForLoop);


function TestNot() {
  "use asm";

  function caller() {
    var a = 0;
    a = !(2 > 3);
    return a | 0;
  }

  return {caller:caller};
}

assertWasm(1, TestNot);


function TestNotEquals() {
  "use asm";

  function caller() {
    var a = 3;
    if ((a|0) != 2) {
      return 21;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(21, TestNotEquals);


function TestUnsignedComparison() {
  "use asm";

  function caller() {
    var a = 0xffffffff;
    if ((a>>>0) > (0>>>0)) {
      return 22;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(22, TestUnsignedComparison);


function TestMixedAdd() {
  "use asm";

  function caller() {
    var a = 0x80000000;
    var b = 0x7fffffff;
    var c = 0;
    c = ((a>>>0) + b)|0;
    if ((c >>> 0) > (0>>>0)) {
      if ((c|0) < 0) {
        return 23;
      }
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(23, TestMixedAdd);


function TestInt32HeapAccess(stdlib, foreign, buffer) {
  "use asm";

  var m = new stdlib.Int32Array(buffer);
  function caller() {
    var i = 4;

    m[0] = (i + 1) | 0;
    m[i >> 2] = ((m[0]|0) + 1) | 0;
    m[2] = ((m[i >> 2]|0) + 1) | 0;
    return m[2] | 0;
  }

  return {caller: caller};
}

assertWasm(7, TestInt32HeapAccess);


function TestInt32HeapAccessExternal() {
  var memory = new ArrayBuffer(1024);
  var memory_int32 = new Int32Array(memory);
  var module_decl = eval('(' + TestInt32HeapAccess.toString() + ')');
  var module = module_decl(stdlib, null, memory);
  assertValidAsm(module_decl);
  assertEquals(7, module.caller());
  assertEquals(7, memory_int32[2]);
}

TestInt32HeapAccessExternal();


function TestHeapAccessIntTypes() {
  var types = [
    [Int8Array, 'Int8Array', '>> 0'],
    [Uint8Array, 'Uint8Array', '>> 0'],
    [Int16Array, 'Int16Array', '>> 1'],
    [Uint16Array, 'Uint16Array', '>> 1'],
    [Int32Array, 'Int32Array', '>> 2'],
    [Uint32Array, 'Uint32Array', '>> 2'],
  ];
  for (var i = 0; i < types.length; i++) {
    var code = TestInt32HeapAccess.toString();
    code = code.replace('Int32Array', types[i][1]);
    code = code.replace(/>> 2/g, types[i][2]);
    var memory = new ArrayBuffer(1024);
    var memory_view = new types[i][0](memory);
    var module_decl = eval('(' + code + ')');
    var module = module_decl(stdlib, null, memory);
    assertValidAsm(module_decl);
    assertEquals(7, module.caller());
    assertEquals(7, memory_view[2]);
    assertValidAsm(module_decl);
  }
}

TestHeapAccessIntTypes();


function TestFloatHeapAccess(stdlib, foreign, buffer) {
  "use asm";

  var f32 = new stdlib.Float32Array(buffer);
  var f64 = new stdlib.Float64Array(buffer);
  var fround = stdlib.Math.fround;
  function caller() {
    var i = 8;
    var j = 8;
    var v = 6.0;

    f64[2] = v + 1.0;
    f64[i >> 3] = +f64[2] + 1.0;
    f64[j >> 3] = +f64[j >> 3] + 1.0;
    i = +f64[i >> 3] == 9.0;
    return i|0;
  }

  return {caller: caller};
}

assertWasm(1, TestFloatHeapAccess);


function TestFloatHeapAccessExternal() {
  var memory = new ArrayBuffer(1024);
  var memory_float64 = new Float64Array(memory);
  var module_decl = eval('(' + TestFloatHeapAccess.toString() + ')');
  var module = module_decl(stdlib, null, memory);
  assertValidAsm(module_decl);
  assertEquals(1, module.caller());
  assertEquals(9.0, memory_float64[1]);
}

TestFloatHeapAccessExternal();


function TestConvertI32() {
  "use asm";

  function caller() {
    var a = 1.5;
    if ((~~(a + a)) == 3) {
      return 24;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(24, TestConvertI32);


function TestConvertF64FromInt() {
  "use asm";

  function caller() {
    var a = 1;
    if ((+((a + a)|0)) > 1.5) {
      return 25;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(25, TestConvertF64FromInt);


function TestConvertF64FromUnsigned() {
  "use asm";

  function caller() {
    var a = 0xffffffff;
    if ((+(a>>>0)) > 0.0) {
      if((+(a|0)) < 0.0) {
        return 26;
      }
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(26, TestConvertF64FromUnsigned);


function TestModInt() {
  "use asm";

  function caller() {
    var a = -83;
    var b = 28;
    return ((a|0)%(b|0))|0;
  }

  return {caller:caller};
}

assertWasm(-27,TestModInt);


function TestModUnsignedInt() {
  "use asm";

  function caller() {
    var a = 0x80000000;  //2147483648
    var b = 10;
    return ((a>>>0)%(b>>>0))|0;
  }

  return {caller:caller};
}

assertWasm(8, TestModUnsignedInt);


function TestModDouble() {
  "use asm";

  function caller() {
    var a = 5.25;
    var b = 2.5;
    if (a%b == 0.25) {
      return 28;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(28, TestModDouble);


function TestModDoubleNegative() {
  "use asm";

  function caller() {
    var a = -34359738368.25;
    var b = 2.5;
    if (a%b == -0.75) {
      return 28;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(28, TestModDoubleNegative);


(function () {
function TestNamedFunctions() {
  "use asm";

  var a = 0.0;
  var b = 0.0;

  function add() {
    return +(a + b);
  }

  function init() {
    a = 43.25;
    b = 34.25;
  }

  return {init:init,
          add:add};
}

var module_decl = eval('(' + TestNamedFunctions.toString() + ')');
var module = module_decl(stdlib);
assertValidAsm(module_decl);
module.init();
assertEquals(77.5, module.add());
})();


(function () {
function TestGlobalsWithInit() {
  "use asm";

  var a = 43.25;
  var b = 34.25;

  function add() {
    return +(a + b);
  }

  return {add:add};
}

var module_decl = eval('(' + TestGlobalsWithInit.toString() + ')');
var module = module_decl(stdlib);
assertValidAsm(module_decl);
assertEquals(77.5, module.add());
})();

function TestForLoop() {
  "use asm"

  function caller() {
    var ret = 0;
    var i = 0;
    for (i = 2; (i|0) <= 10; i = (i+1)|0) {
      ret = (ret + i) | 0;
    }
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(54, TestForLoop);


function TestForLoopWithoutInit() {
  "use asm"

  function caller() {
    var ret = 0;
    var i = 0;
    for (; (i|0) < 10; i = (i+1)|0) {
      ret = (ret + 10) | 0;
    }
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(100,TestForLoopWithoutInit);


function TestForLoopWithoutCondition() {
  "use asm"

  function caller() {
    var ret = 0;
    var i = 0;
    for (i=1;; i = (i+1)|0) {
      ret = (ret + i) | 0;
      if ((i|0) == 11) {
        break;
      }
    }
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(66, TestForLoopWithoutCondition);


function TestForLoopWithoutNext() {
  "use asm"

  function caller() {
    var i = 0;
    for (i=1; (i|0) < 41;) {
      i = (i + 1) | 0;
    }
    return i|0;
  }

  return {caller:caller};
}

assertWasm(41, TestForLoopWithoutNext);


function TestForLoopWithoutBody() {
  "use asm"

  function caller() {
    var i = 0;
    for (i=1; (i|0) < 45 ; i = (i+1)|0) {
    }
    return i|0;
  }

  return {caller:caller};
}

assertWasm(45, TestForLoopWithoutBody);


function TestDoWhile() {
  "use asm"

  function caller() {
    var i = 0;
    var ret = 21;
    do {
      ret = (ret + ret)|0;
      i = (i + 1)|0;
    } while ((i|0) < 2);
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(84, TestDoWhile);


function TestConditional() {
  "use asm"

  function caller() {
    var x = 1;
    return (((x|0) > 0) ? 41 : 71)|0;
  }

  return {caller:caller};
}

assertWasm(41, TestConditional);


function TestInitFunctionWithNoGlobals() {
  "use asm";
  function caller() {
    return 51;
  }
  return {caller:caller};
}

assertWasm(51, TestInitFunctionWithNoGlobals);


(function () {
function TestExportNameDifferentFromFunctionName() {
  "use asm";
  function caller() {
    return 55;
  }
  return {alt_caller:caller};
}

var module_decl = eval(
  '(' + TestExportNameDifferentFromFunctionName.toString() + ')');
var module = module_decl(stdlib);
assertValidAsm(module_decl);
assertEquals(55, module.alt_caller());
})();


function TestFunctionTableSingleFunction() {
  "use asm";

  function dummy() {
    return 71;
  }

  function caller() {
    // TODO(jpp): the parser optimizes function_table[0&0] to function table[0].
    var v = 0;
    return function_table[v&0]() | 0;
  }

  var function_table = [dummy]

  return {caller:caller};
}

assertWasm(71, TestFunctionTableSingleFunction);


function TestFunctionTableMultipleFunctions() {
  "use asm";

  function inc1(x) {
    x = x|0;
    return (x+1)|0;
  }

  function inc2(x) {
    x = x|0;
    return (x+2)|0;
  }

  function caller() {
    var i = 0, j = 1;
    if ((function_table[i&1](50)|0) == 51) {
      if ((function_table[j&1](60)|0) == 62) {
        return 73;
      }
    }
    return 0;
  }

  var function_table = [inc1, inc2]

  return {caller:caller};
}

assertWasm(73, TestFunctionTableMultipleFunctions);


(function () {
function TestFunctionTable(stdlib, foreign, buffer) {
  "use asm";

  function add(a, b) {
    a = a|0;
    b = b|0;
    return (a+b)|0;
  }

  function sub(a, b) {
    a = a|0;
    b = b|0;
    return (a-b)|0;
  }

  function inc(a) {
    a = a|0;
    return (a+1)|0;
  }

  function caller(table_id, fun_id, arg1, arg2) {
    table_id = table_id|0;
    fun_id = fun_id|0;
    arg1 = arg1|0;
    arg2 = arg2|0;
    if ((table_id|0) == 0) {
      return funBin[fun_id&3](arg1, arg2)|0;
    } else if ((table_id|0) == 1) {
      return fun[fun_id&0](arg1)|0;
    }
    return 0;
  }

  var funBin = [add, sub, sub, add];
  var fun = [inc];

  return {caller:caller};
}

print("TestFunctionTable...");
var module = TestFunctionTable(stdlib);
assertEquals(55, module.caller(0, 0, 33, 22));
assertEquals(11, module.caller(0, 1, 33, 22));
assertEquals(9, module.caller(0, 2, 54, 45));
assertEquals(99, module.caller(0, 3, 54, 45));
assertEquals(23, module.caller(0, 4, 12, 11));
assertEquals(31, module.caller(1, 0, 30, 11));
})();


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


(function() {
  function TestByteHeapAccessCompat(stdlib, foreign, buffer) {
    "use asm";

    var HEAP8 = new stdlib.Uint8Array(buffer);
    var HEAP32 = new stdlib.Int32Array(buffer);

    function store(i, v) {
      i = i | 0;
      v = v | 0;
      HEAP32[i >> 2] = v;
    }

    function storeb(i, v) {
      i = i | 0;
      v = v | 0;
      HEAP8[i | 0] = v;
    }

    function load(i) {
      i = i | 0;
      return HEAP8[i] | 0;
    }

    function iload(i) {
      i = i | 0;
      return HEAP8[HEAP32[i >> 2] | 0] | 0;
    }

    return {load: load, iload: iload, store: store, storeb: storeb};
  }

  var memory = new ArrayBuffer(1024);
  var module_decl = eval('(' + TestByteHeapAccessCompat.toString() + ')');
  var m = module_decl(stdlib, null, memory);
  assertValidAsm(module_decl);
  m.store(0, 20);
  m.store(4, 21);
  m.store(8, 22);
  m.storeb(20, 123);
  m.storeb(21, 42);
  m.storeb(22, 77);
  assertEquals(123, m.load(20));
  assertEquals(42, m.load(21));
  assertEquals(77, m.load(22));
  assertEquals(123, m.iload(0));
  assertEquals(42, m.iload(4));
  assertEquals(77, m.iload(8));
})();


function TestGlobalBlock(stdlib, foreign, buffer) {
  "use asm";

  var x = foreign.x | 0, y = foreign.y | 0;

  function test() {
    return (x + y) | 0;
  }

  return {caller: test};
}

assertWasm(15, TestGlobalBlock, { x: 4, y: 11 });

(function TestComma() {
  function CommaModule() {
    "use asm";

    function ifunc(a, b) {
      a = +a;
      b = b | 0;
      return (a, b) | 0;
    }

    function dfunc(a, b) {
      a = a | 0;
      b = +b;
      return +(a, b);
    }

    return {ifunc: ifunc, dfunc: dfunc};
  }

  var module_decl = eval('(' + CommaModule.toString() + ')');
  var m = module_decl(stdlib);
  assertValidAsm(module_decl);
  assertEquals(123, m.ifunc(456.7, 123));
  assertEquals(123.4, m.dfunc(456, 123.4));
})();


function TestFloatAsDouble(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  function func() {
    var x = fround(1.0);
    return +fround(x);
  }
  return {caller: func};
}
assertWasm(1, TestFloatAsDouble);


function TestOr() {
  "use asm";
  function func() {
    var x = 1;
    var y = 2;
    return (x | y) | 0;
  }
  return {caller: func};
}

assertWasm(3, TestOr);


function TestAnd() {
  "use asm";
  function func() {
    var x = 3;
    var y = 2;
    return (x & y) | 0;
  }
  return {caller: func};
}

assertWasm(2, TestAnd);


function TestXor() {
  "use asm";
  function func() {
    var x = 3;
    var y = 2;
    return (x ^ y) | 0;
  }
  return {caller: func};
}

assertWasm(1, TestXor);


function TestIntishAssignment(stdlib, foreign, heap) {
  "use asm";
  var HEAP32 = new stdlib.Int32Array(heap);
  function func() {
    var a = 1;
    var b = 2;
    HEAP32[0] = a + b;
    return HEAP32[0] | 0;
  }
  return {caller: func};
}

assertWasm(3, TestIntishAssignment);


function TestFloatishAssignment(stdlib, foreign, heap) {
  "use asm";
  var HEAPF32 = new stdlib.Float32Array(heap);
  var fround = stdlib.Math.fround;
  function func() {
    var a = fround(1.0);
    var b = fround(2.0);
    HEAPF32[0] = a + b;
    return +HEAPF32[0];
  }
  return {caller: func};
}

assertWasm(3, TestFloatishAssignment);


function TestDoubleToFloatAssignment(stdlib, foreign, heap) {
  "use asm";
  var HEAPF32 = new stdlib.Float32Array(heap);
  var fround = stdlib.Math.fround;
  function func() {
    var a = 1.23;
    HEAPF32[0] = a;
    return +HEAPF32[0];
  }
  return {caller: func};
}

assertWasm(Math.fround(1.23), TestDoubleToFloatAssignment);


function TestIntegerMultiplyBothWays(stdlib, foreign, heap) {
  "use asm";
  function func() {
    var a = 1;
    return (((a * 3)|0) + ((4 * a)|0)) | 0;
  }
  return {caller: func};
}

assertWasm(7, TestIntegerMultiplyBothWays);


(function TestBadAssignDoubleFromIntish() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    function func() {
      var a = 1;
      var b = 3.0;
      b = a;
    }
    return {func: func};
  }
  print("TestBadAssignDoubleFromIntish...");
  Module(stdlib);
  assertFalse(%IsAsmWasmCode(Module));
})();


(function TestBadAssignIntFromDouble() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    function func() {
      var a = 1;
      var b = 3.0;
      a = b;
    }
    return {func: func};
  }
  print("TestBadAssignIntFromDouble...");
  Module(stdlib);
  assertFalse(%IsAsmWasmCode(Module));
})();


(function TestBadMultiplyIntish() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    function func() {
      var a = 1;
      return ((a + a) * 4) | 0;
    }
    return {func: func};
  }
  print("TestBadMultiplyIntish...");
  Module(stdlib);
  assertFalse(%IsAsmWasmCode(Module));
})();


(function TestBadCastFromInt() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    function func() {
      var a = 1;
      return +a;
    }
    return {func: func};
  }
  print("TestBadCastFromInt...");
  Module(stdlib);
  assertFalse(%IsAsmWasmCode(Module));
})();


function TestAndNegative() {
  "use asm";
  function func() {
    var x = 1;
    var y = 2;
    var z = 0;
    z = x + y & -1;
    return z | 0;
  }
  return {caller: func};
}

assertWasm(3, TestAndNegative);


function TestNegativeDouble() {
  "use asm";
  function func() {
    var x = -34359738368.25;
    var y = -2.5;
    return +(x + y);
  }
  return {caller: func};
}

assertWasm(-34359738370.75, TestNegativeDouble);


(function TestBadAndDouble() {
  function Module() {
    "use asm";
    function func() {
      var x = 1.0;
      var y = 2.0;
      return (x & y) | 0;
    }
    return {func: func};
  }

  Module(stdlib);
  assertFalse(%IsAsmWasmCode(Module));
})();


(function TestBadExportKey() {
  function Module() {
    "use asm";
    function func() {
    }
    return {123: func};
  }

  Module(stdlib);
  assertFalse(%IsAsmWasmCode(Module));
})();


/*
// TODO(bradnelson): Technically invalid, but useful to cover unicode, revises
// and re-enable.
(function TestUnicodeExportKey() {
  function Module() {
    "use asm";
    function func() {
      return 42;
    }
    return {"\u00d1\u00e6": func};
  }

  var m = Module(stdlib);
  assertEquals(42, m.Ñæ());
  assertValidAsm(Module);
})();
*/


function TestAndIntAndHeapValue(stdlib, foreign, buffer) {
  "use asm";
  var HEAP32 = new stdlib.Int32Array(buffer);
  function func() {
    var x = 0;
    x = HEAP32[0] & -1;
    return x | 0;
  }
  return {caller: func};
}

assertWasm(0, TestAndIntAndHeapValue);


function TestOutOfBoundsConversion($a,$b,$c){'use asm';
  function aaa() {
    var f = 0.0;
    var a = 0;
    f = 5616315000.000001;
    a = ~~f >>>0;
    return a | 0;
  }
  return { caller : aaa };
}

assertWasm(1321347704, TestOutOfBoundsConversion);


(function TestUnsignedLiterals() {
  function asmModule() {
    "use asm";
    function u0xffffffff() {
      var f = 0xffffffff;
      return +(f >>> 0);
    }
    function u0x80000000() {
      var f = 0x80000000;
      return +(f >>> 0);
    }
    function u0x87654321() {
      var f = 0x87654321;
      return +(f >>> 0);
    }
    return {
      u0xffffffff: u0xffffffff,
      u0x80000000: u0x80000000,
      u0x87654321: u0x87654321,
    };
  }
  var decl = eval('(' + asmModule.toString() + ')');
  var wasm = decl(stdlib);
  assertValidAsm(decl);
  assertEquals(0xffffffff, wasm.u0xffffffff());
  assertEquals(0x80000000, wasm.u0x80000000());
  assertEquals(0x87654321, wasm.u0x87654321());
})();


function TestIfWithUnsigned() {
  "use asm";
  function main() {
    if (2147483658) { // 2^31 + 10
      return 231;
    }
    return 0;
  }
  return {caller:main};
}

assertWasm(231, TestIfWithUnsigned);


function TestLoopsWithUnsigned() {
  "use asm";
  function main() {
    var val = 1;
    var count = 0;
    for (val = 2147483648; 2147483648;) {
      val = 2147483649;
      break;
    }
    while (val>>>0) {
      val = (val + 1) | 0;
      count = (count + 1)|0;
      if ((count|0) == 9) {
        break;
      }
    }
    count = 0;
    do {
      val = (val + 2) | 0;
      count = (count + 1)|0;
      if ((count|0) == 5) {
        break;
      }
    } while (0xffffffff);
    if ((val>>>0) == 2147483668) {
      return 323;
    }
    return 0;
  }
  return {caller:main};
}

assertWasm(323, TestLoopsWithUnsigned);


function TestSingleFunctionModule() {
  "use asm";
  function add(a, b) {
    a = a | 0;
    b = b | 0;
    return (a + b) | 0;
  }
  return add;
}

assertEquals(7, TestSingleFunctionModule()(3, 4));


function TestNotZero() {
  "use asm";
  function caller() {
    if (!0) {
      return 44;
    } else {
      return 55;
    }
    return 0;
  }
  return {caller: caller};
}

assertWasm(44, TestNotZero);


function TestNotOne() {
  "use asm";
  function caller() {
    if (!1) {
      return 44;
    } else {
      return 55;
    }
    return 0;
  }
  return {caller: caller};
}

assertWasm(55, TestNotOne);


function TestDotfulFloat(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  var foo = fround(55.0);
  function caller() {
    return +foo;
  }
  return {caller: caller};
}

assertWasm(55, TestDotfulFloat);


function TestDotfulLocalFloat(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  function caller() {
    var foo = fround(55.0);
    return +foo;
  }
  return {caller: caller};
}

assertWasm(55, TestDotfulLocalFloat);


function TestDotlessFloat(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  var foo = fround(55);
  function caller() {
    return +foo;
  }
  return {caller: caller};
}

assertWasm(55, TestDotlessFloat);


function TestDotlessLocalFloat(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  function caller() {
    var foo = fround(55);
    return +foo;
  }
  return {caller: caller};
}

assertWasm(55, TestDotlessLocalFloat);


function TestFloatGlobals(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  var foo = fround(1.25);
  function caller() {
    foo = fround(foo + fround(1.0));
    foo = fround(foo + fround(1.0));
    return +foo;
  }
  return {caller: caller};
}

assertWasm(3.25, TestFloatGlobals);


(function TestExportTwice() {
  function asmModule() {
    "use asm";
    function foo() {
      return 42;
    }
    return {bar: foo, baz: foo};
  }
  var m = asmModule();
  assertEquals(42, m.bar());
  assertEquals(42, m.baz());
})();
