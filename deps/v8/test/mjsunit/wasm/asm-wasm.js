// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

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

assertEquals(11, _WASMEXP_.asmCompileRun(EmptyTest.toString()));

function IntTest() {
  "use asm";
  function sum(a, b) {
    a = a|0;
    b = b|0;
    var c = (b + 1)|0
    var d = 3.0;
    var e = d | 0;  // double conversion
    return (a + c + 1)|0;
  }

  function caller() {
    return sum(77,22) | 0;
  }

  return {caller: caller};
}

assertEquals(101, _WASMEXP_.asmCompileRun(IntTest.toString()));

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

assertEquals(1, _WASMEXP_.asmCompileRun(Float64Test.toString()));

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
  _WASMEXP_.asmCompileRun(BadModule.toString())
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

assertEquals(1, _WASMEXP_.asmCompileRun(TestReturnInBlock.toString()));

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

assertEquals(5, _WASMEXP_.asmCompileRun(TestWhileSimple.toString()));

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

assertEquals(4, _WASMEXP_.asmCompileRun(TestWhileWithoutBraces.toString()));

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

assertEquals(6, _WASMEXP_.asmCompileRun(TestReturnInWhile.toString()));

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

assertEquals(7, _WASMEXP_.asmCompileRun(TestReturnInWhileWithoutBraces.toString()));

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

assertEquals(8, _WASMEXP_.asmCompileRun(TestBreakInWhile.toString()));

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

assertEquals(9, _WASMEXP_.asmCompileRun(TestBreakInNestedWhile.toString()));

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

assertEquals(10, _WASMEXP_.asmCompileRun(TestBreakInBlock.toString()));

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

assertEquals(11, _WASMEXP_.asmCompileRun(TestBreakInNamedWhile.toString()));

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

assertEquals(-5, _WASMEXP_.asmCompileRun(TestContinue.toString()));

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

assertEquals(20, _WASMEXP_.asmCompileRun(TestContinueInNamedWhile.toString()));

function TestNot() {
  "use asm";

  function caller() {
    var a = !(2 > 3);
    return a | 0;
  }

  return {caller:caller};
}

assertEquals(1, _WASMEXP_.asmCompileRun(TestNot.toString()));

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

assertEquals(21, _WASMEXP_.asmCompileRun(TestNotEquals.toString()));

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

assertEquals(22, _WASMEXP_.asmCompileRun(TestUnsignedComparison.toString()));

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

assertEquals(23, _WASMEXP_.asmCompileRun(TestMixedAdd.toString()));

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

assertEquals(7, _WASMEXP_.asmCompileRun(TestInt32HeapAccess.toString()));

function TestHeapAccessIntTypes() {
  var types = [
    ['Int8Array', '>> 0'],
    ['Uint8Array', '>> 0'],
    ['Int16Array', '>> 1'],
    ['Uint16Array', '>> 1'],
    ['Int32Array', '>> 2'],
    ['Uint32Array', '>> 2'],
  ];
  for (var i = 0; i < types.length; i++) {
    var code = TestInt32HeapAccess.toString();
    code = code.replace('Int32Array', types[i][0]);
    code = code.replace(/>> 2/g, types[i][1]);
    assertEquals(7, _WASMEXP_.asmCompileRun(code));
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

    // TODO(bradnelson): Add float32 when asm-wasm supports it.
    f64[2] = v + 1.0;
    f64[i >> 3] = +f64[2] + 1.0;
    f64[j >> 3] = +f64[j >> 3] + 1.0;
    i = +f64[i >> 3] == 9.0;
    return i|0;
  }

  return {caller: caller};
}

assertEquals(1, _WASMEXP_.asmCompileRun(TestFloatHeapAccess.toString()));

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

assertEquals(24, _WASMEXP_.asmCompileRun(TestConvertI32.toString()));

function TestConvertF64FromInt() {
  "use asm";

  function caller() {
    var a = 1;
    if ((+(a + a)) > 1.5) {
      return 25;
    }
    return 0;
  }

  return {caller:caller};
}

assertEquals(25, _WASMEXP_.asmCompileRun(TestConvertF64FromInt.toString()));

function TestConvertF64FromUnsigned() {
  "use asm";

  function caller() {
    var a = 0xffffffff;
    if ((+(a>>>0)) > 0.0) {
      if((+a) < 0.0) {
        return 26;
      }
    }
    return 0;
  }

  return {caller:caller};
}

assertEquals(26, _WASMEXP_.asmCompileRun(TestConvertF64FromUnsigned.toString()));

function TestModInt() {
  "use asm";

  function caller() {
    var a = -83;
    var b = 28;
    return ((a|0)%(b|0))|0;
  }

  return {caller:caller};
}

assertEquals(-27, _WASMEXP_.asmCompileRun(TestModInt.toString()));

function TestModUnsignedInt() {
  "use asm";

  function caller() {
    var a = 0x80000000;  //2147483648
    var b = 10;
    return ((a>>>0)%(b>>>0))|0;
  }

  return {caller:caller};
}

assertEquals(8, _WASMEXP_.asmCompileRun(TestModUnsignedInt.toString()));

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

assertEquals(28, _WASMEXP_.asmCompileRun(TestModDouble.toString()));

/*
TODO: Fix parsing of negative doubles
      Fix code to use trunc instead of casts
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

assertEquals(28, _WASMEXP_.asmCompileRun(TestModDoubleNegative.toString()));
*/

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

var module = _WASMEXP_.instantiateModuleFromAsm(TestNamedFunctions.toString());
module.init();
assertEquals(77.5, module.add());

function TestGlobalsWithInit() {
  "use asm";

  var a = 43.25;
  var b = 34.25;

  function add() {
    return +(a + b);
  }

  return {add:add};
}

var module = _WASMEXP_.instantiateModuleFromAsm(TestGlobalsWithInit.toString());
module.__init__();
assertEquals(77.5, module.add());

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

assertEquals(54, _WASMEXP_.asmCompileRun(TestForLoop.toString()));

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

assertEquals(100, _WASMEXP_.asmCompileRun(TestForLoopWithoutInit.toString()));

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

assertEquals(66, _WASMEXP_.asmCompileRun(TestForLoopWithoutCondition.toString()));

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

assertEquals(41, _WASMEXP_.asmCompileRun(TestForLoopWithoutNext.toString()));

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

assertEquals(45, _WASMEXP_.asmCompileRun(TestForLoopWithoutBody.toString()));

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

assertEquals(84, _WASMEXP_.asmCompileRun(TestDoWhile.toString()));

function TestConditional() {
  "use asm"

  function caller() {
    var x = 1;
    return ((x > 0) ? 41 : 71)|0;
  }

  return {caller:caller};
}

assertEquals(41, _WASMEXP_.asmCompileRun(TestConditional.toString()));

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

assertEquals(23, _WASMEXP_.asmCompileRun(TestSwitch.toString()));

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

assertEquals(42, _WASMEXP_.asmCompileRun(TestSwitchFallthrough.toString()));

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

assertEquals(43, _WASMEXP_.asmCompileRun(TestNestedSwitch.toString()));

function TestInitFunctionWithNoGlobals() {
  "use asm";
  function caller() {
    return 51;
  }
  return {caller};
}

var module = _WASMEXP_.instantiateModuleFromAsm(
    TestInitFunctionWithNoGlobals.toString());
module.__init__();
assertEquals(51, module.caller());

function TestExportNameDifferentFromFunctionName() {
  "use asm";
  function caller() {
    return 55;
  }
  return {alt_caller:caller};
}

var module = _WASMEXP_.instantiateModuleFromAsm(
    TestExportNameDifferentFromFunctionName.toString());
module.__init__();
assertEquals(55, module.alt_caller());
