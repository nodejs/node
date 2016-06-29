// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

function assertWasm(expected, func, ffi) {
  print("Testing " + func.name + "...");
  assertEquals(expected, Wasm.instantiateModuleFromAsm(
      func.toString(), ffi).caller());
}

function EmptyTest() {
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


function IntTest() {
  "use asm";
  function sum(a, b) {
    a = a|0;
    b = b|0;
    var c = (b + 1)|0
    var d = 3.0;
    var e = ~~d;  // double conversion
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
    var a = +sum(70.1,10.2);
    var ret = 0|0;
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
    var c = (b + 1)|0
    return (a + c + 1)|0;
  }

  function caller() {
    return call(1, 2)|0;
  }

  return {caller: caller};
}

assertThrows(function() {
  Wasm.instantiateModuleFromAsm(BadModule.toString()).caller();
});


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
    while(x < 5) {
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
    while(x <= 3)
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
    while(x < 10) {
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
    while(x < 5)
      return 7;
    return x|0;
  }

  return {caller: caller};
}

assertWasm(7, TestReturnInWhileWithoutBraces);


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


function TestBreakInNestedWhile() {
  "use asm";

  function caller() {
    var x = 1.0;
    while(x < 1.5) {
      while(1)
        break;
      x = +(x + 0.25);
    }
    var ret = 0;
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
      if (x == 10) {
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
      while (x == 11) {
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
    while (x >= 0) {
      x = (x - 1)|0;
      if (x == 2) {
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
    outer: while (x > 0) {
      x = (x - 1)|0;
      y = 0;
      while (y < 5) {
        if (x == 3) {
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


function TestNot() {
  "use asm";

  function caller() {
    var a = !(2 > 3);
    return a | 0;
  }

  return {caller:caller};
}

assertWasm(1, TestNot);


function TestNotEquals() {
  "use asm";

  function caller() {
    var a = 3;
    if (a != 2) {
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
      if (c < 0) {
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
  var module = Wasm.instantiateModuleFromAsm(
      TestInt32HeapAccess.toString(), null, memory);
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
    var module = Wasm.instantiateModuleFromAsm(code, null, memory);
    assertEquals(7, module.caller());
    assertEquals(7, memory_view[2]);
    assertEquals(7, Wasm.instantiateModuleFromAsm(code).caller());
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

assertEquals(1, Wasm.instantiateModuleFromAsm(
      TestFloatHeapAccess.toString()).caller());


function TestFloatHeapAccessExternal() {
  var memory = new ArrayBuffer(1024);
  var memory_float64 = new Float64Array(memory);
  var module = Wasm.instantiateModuleFromAsm(
      TestFloatHeapAccess.toString(), null, memory);
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

var module = Wasm.instantiateModuleFromAsm(TestNamedFunctions.toString());
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

var module = Wasm.instantiateModuleFromAsm(TestGlobalsWithInit.toString());
assertEquals(77.5, module.add());
})();

function TestForLoop() {
  "use asm"

  function caller() {
    var ret = 0;
    var i = 0;
    for (i = 2; i <= 10; i = (i+1)|0) {
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
    for (; i < 10; i = (i+1)|0) {
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
      if (i == 11) {
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
    for (i=1; i < 41;) {
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
    for (i=1; i < 45 ; i = (i+1)|0) {
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
    } while (i < 2);
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(84, TestDoWhile);


function TestConditional() {
  "use asm"

  function caller() {
    var x = 1;
    return ((x > 0) ? 41 : 71)|0;
  }

  return {caller:caller};
}

assertWasm(41, TestConditional);


function TestSwitch() {
  "use asm"

  function caller() {
    var ret = 0;
    var x = 7;
    switch (x) {
      case 1: return 0;
      case 7: {
        ret = 12;
        break;
      }
      default: return 0;
    }
    switch (x) {
      case 1: return 0;
      case 8: return 0;
      default: ret = (ret + 11)|0;
    }
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(23, TestSwitch);


function TestSwitchFallthrough() {
  "use asm"

  function caller() {
    var x = 17;
    var ret = 0;
    switch (x) {
      case 17:
      case 14: ret = 39;
      case 1: ret = (ret + 3)|0;
      case 4: break;
      default: ret = (ret + 1)|0;
    }
    return ret|0;
  }

  return {caller:caller};
}

assertWasm(42, TestSwitchFallthrough);


function TestNestedSwitch() {
  "use asm"

  function caller() {
    var x = 3;
    var y = -13;
    switch (x) {
      case 1: return 0;
      case 3: {
        switch (y) {
          case 2: return 0;
          case -13: return 43;
          default: return 0;
        }
      }
      default: return 0;
    }
    return 0;
  }

  return {caller:caller};
}

assertWasm(43, TestNestedSwitch);


(function () {
function TestInitFunctionWithNoGlobals() {
  "use asm";
  function caller() {
    return 51;
  }
  return {caller};
}

var module = Wasm.instantiateModuleFromAsm(
    TestInitFunctionWithNoGlobals.toString());
assertEquals(51, module.caller());
})();

(function () {
function TestExportNameDifferentFromFunctionName() {
  "use asm";
  function caller() {
    return 55;
  }
  return {alt_caller:caller};
}

var module = Wasm.instantiateModuleFromAsm(
    TestExportNameDifferentFromFunctionName.toString());
assertEquals(55, module.alt_caller());
})();

function TestFunctionTableSingleFunction() {
  "use asm";

  function dummy() {
    return 71;
  }

  function caller() {
    return function_table[0&0]() | 0;
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
    if (function_table[0&1](50) == 51) {
      if (function_table[1&1](60) == 62) {
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
function TestFunctionTable() {
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
    if (table_id == 0) {
      return funBin[fun_id&3](arg1, arg2)|0;
    } else if (table_id == 1) {
      return fun[fun_id&0](arg1)|0;
    }
    return 0;
  }

  var funBin = [add, sub, sub, add];
  var fun = [inc];

  return {caller:caller};
}

var module = Wasm.instantiateModuleFromAsm(TestFunctionTable.toString());
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

  var module = Wasm.instantiateModuleFromAsm(AsmModule.toString(),
                                                  foreign, null);

  assertEquals(103, module.caller(23, 103));
}

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

  var module = Wasm.instantiateModuleFromAsm(AsmModule.toString(),
                                                  foreign, null);

  assertEquals(89, module.caller(83, 83.25));
}

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
    var module = Wasm.instantiateModuleFromAsm(
        AsmModule.toString(), env);
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
  // Check that an undefined ffi is ok.
  TestCase(undefined, 0, NaN, 0, NaN);
  // Check that true values are converted properly.
  TestCase({foo: true, bar: true, baz: true}, 1, 1.0, 1, 1.0);
  // Check that false values are converted properly.
  TestCase({foo: false, bar: false, baz: false}, 0, 0, 0, 0);
  // Check that null values are converted properly.
  TestCase({foo: null, bar: null, baz: null}, 0, 0, 0, 0);
  // Check that string values are converted properly.
  TestCase({foo: 'hi', bar: 'there', baz: 'dude'}, 0, NaN, 0, NaN);
  TestCase({foo: '0xff', bar: '234', baz: '456.1'}, 255, 234, 456, 456.1, 456);
  // Check that Date values are converted properly.
  TestCase({foo: new Date(123), bar: new Date(456),
            baz: new Date(789)}, 123, 456, 789, 789);
  // Check that list values are converted properly.
  TestCase({foo: [], bar: [], baz: []}, 0, 0, 0, 0);
  // Check that object values are converted properly.
  TestCase({foo: {}, bar: {}, baz: {}}, 0, NaN, 0, NaN);
  // Check that getter object values are converted properly.
  var o = {
    get foo() {
      return 123.4;
    }
  };
  TestCase({foo: o.foo, bar: o.foo, baz: o.foo}, 123, 123.4, 123, 123.4);
  // Check that getter object values are converted properly.
  var o = {
    get baz() {
      return 123.4;
    }
  };
  TestCase(o, 0, NaN, 123, 123.4);
  // Check that objects with valueOf are converted properly.
  var o = {
    valueOf: function() { return 99; }
  };
  TestCase({foo: o, bar: o, baz: o}, 99, 99, 99, 99);
  // Check that function values are converted properly.
  TestCase({foo: TestCase, bar: TestCase, qux: TestCase}, 0, NaN, 0, NaN);
  // Check that a missing ffi object is safe.
  TestCase(undefined, 0, NaN, 0, NaN);
}

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

  var m = Wasm.instantiateModuleFromAsm(
      TestByteHeapAccessCompat.toString());
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

  var m = Wasm.instantiateModuleFromAsm(CommaModule.toString());
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


(function TestIntishAssignment() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    function func() {
      var a = 1;
      var b = 2;
      HEAP32[0] = a + b;
      return HEAP32[0] | 0;
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(3, m.func());
})();


(function TestFloatishAssignment() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAPF32 = new stdlib.Float32Array(heap);
    var fround = stdlib.Math.fround;
    function func() {
      var a = fround(1.0);
      var b = fround(2.0);
      HEAPF32[0] = a + b;
      return +HEAPF32[0];
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(3, m.func());
})();


(function TestDoubleToFloatAssignment() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAPF32 = new stdlib.Float32Array(heap);
    var fround = stdlib.Math.fround;
    function func() {
      var a = 1.23;
      HEAPF32[0] = a;
      return +HEAPF32[0];
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(1.23, m.func());
});


(function TestIntegerMultiplyBothWays() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    function func() {
      var a = 1;
      return ((a * 3) + (4 * a)) | 0;
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(7, m.func());
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
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(Module.toString());
  });
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
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(Module.toString());
  });
})();


(function TestAndNegative() {
  function Module() {
    "use asm";
    function func() {
      var x = 1;
      var y = 2;
      var z = 0;
      z = x + y & -1;
      return z | 0;
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(3, m.func());
})();


(function TestNegativeDouble() {
  function Module() {
    "use asm";
    function func() {
      var x = -(34359738368.25);
      var y = -2.5;
      return +(x + y);
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(-34359738370.75, m.func());
})();


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

  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(Module.toString());
  });
})();


(function TestAndIntAndHeapValue() {
  function Module(stdlib, foreign, buffer) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(buffer);
    function func() {
      var x = 0;
      x = HEAP32[0] & -1;
      return x | 0;
    }
    return {func: func};
  }

  var m = Wasm.instantiateModuleFromAsm(Module.toString());
  assertEquals(0, m.func());
})();

(function TestOutOfBoundsConversion() {
  function asmModule($a,$b,$c){'use asm';
    function aaa() {
      var f = 0.0;
      var a = 0;
      f = 5616315000.000001;
      a = ~~f >>>0;
      return a | 0;
    }
    return { main : aaa };
  }
  var wasm = Wasm.instantiateModuleFromAsm(asmModule.toString());
  assertEquals(1321347704, wasm.main());
})();

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
  var wasm = Wasm.instantiateModuleFromAsm(asmModule.toString());
  assertEquals(0xffffffff, wasm.u0xffffffff());
  assertEquals(0x80000000, wasm.u0x80000000());
  assertEquals(0x87654321, wasm.u0x87654321());
})();
