// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function assertValidAsm(func) {
  assertTrue(%IsAsmWasmCode(func));
}

(function TestSwitch0() {
  function asmModule() {
    "use asm"

    function caller() {
      var ret = 0;
      var x = 7;
      switch (x|0) {
        case 1: {
          return 0;
        }
        case 7: {
          ret = 5;
          break;
        }
        default: return 0;
      }
      return ret|0;
    }

    return {caller:caller};
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(5, wasm.caller());
})();

(function TestSwitch() {
  function asmModule() {
    "use asm"

    function caller() {
      var ret = 0;
      var x = 7;
      switch (x|0) {
        case 1: return 0;
        case 7: {
          ret = 12;
          break;
        }
        default: return 0;
      }
      switch (x|0) {
        case 1: return 0;
        case 8: return 0;
        default: ret = (ret + 11)|0;
      }
      return ret|0;
    }

    return {caller:caller};
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(23, wasm.caller());
})();

(function TestSwitchFallthrough() {
  function asmModule() {
    "use asm"

    function caller() {
      var x = 17;
      var ret = 0;
      switch (x|0) {
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
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(42, wasm.caller());
})();

(function TestNestedSwitch() {
  function asmModule() {
    "use asm"

    function caller() {
      var x = 3;
      var y = -13;
      switch (x|0) {
        case 1: return 0;
        case 3: {
          switch (y|0) {
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
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(43, wasm.caller());
})();

(function TestSwitchWithDefaultOnly() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      switch(x|0) {
        default: return -10;
      }
      return 0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(-10, wasm.main(2));
  assertEquals(-10, wasm.main(54));
})();

(function TestEmptySwitch() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      switch(x|0) {
      }
      return 73;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(73, wasm.main(7));
})();

(function TestSwitchWithBrTable() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      switch(x|0) {
        case 14: return 23;
        case 12: return 25;
        case 15: return 29;
        case 19: return 34;
        case 18: return 17;
        case 16: return 16;
        default: return -1;
      }
      return 0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(25, wasm.main(12));
  assertEquals(23, wasm.main(14));
  assertEquals(29, wasm.main(15));
  assertEquals(16, wasm.main(16));
  assertEquals(17, wasm.main(18));
  assertEquals(34, wasm.main(19));
  assertEquals(-1, wasm.main(-1));
})();

(function TestSwitchWithBalancedTree() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      switch(x|0) {
        case 5: return 52;
        case 1: return 11;
        case 6: return 63;
        case 9: return 19;
        case -4: return -4;
      }
      return 0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(-4, wasm.main(-4));
  assertEquals(11, wasm.main(1));
  assertEquals(52, wasm.main(5));
  assertEquals(63, wasm.main(6));
  assertEquals(19, wasm.main(9));
  assertEquals(0, wasm.main(11));
})();

(function TestSwitchHybrid() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      switch(x|0) {
        case 1: return -4;
        case 2: return 23;
        case 3: return 32;
        case 4: return 14;
        case 7: return 17;
        case 10: return 10;
        case 11: return 121;
        case 12: return 112;
        case 13: return 31;
        case 16: return 16;
        default: return -1;
      }
      return 0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(-4, wasm.main(1));
  assertEquals(23, wasm.main(2));
  assertEquals(32, wasm.main(3));
  assertEquals(14, wasm.main(4));
  assertEquals(17, wasm.main(7));
  assertEquals(10, wasm.main(10));
  assertEquals(121, wasm.main(11));
  assertEquals(112, wasm.main(12));
  assertEquals(31, wasm.main(13));
  assertEquals(16, wasm.main(16));
  assertEquals(-1, wasm.main(20));
})();

(function TestSwitchFallthroughWithBrTable() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      var ret = 0;
      switch(x|0) {
        case 1: {
          ret = 21;
          break;
        }
        case 2: {
          ret = 12;
          break;
        }
        case 3: {
          ret = 43;
        }
        case 4: {
          ret = 54;
          break;
        }
        default: {
          ret = 10;
          break;
        }
      }
      return ret|0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(12, wasm.main(2));
  assertEquals(10, wasm.main(10));
  assertEquals(54, wasm.main(3));
})();

(function TestSwitchFallthroughHybrid() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      var ret = 0;
      switch(x|0) {
        case 1: {
          ret = 1;
          break;
        }
        case 2: {
          ret = 2;
          break;
        }
        case 3: {
          ret = 3;
          break;
        }
        case 4: {
          ret = 4;
        }
        case 7: {
          ret = 7;
          break;
        }
        case 10: {
          ret = 10;
        }
        case 16: {
          ret = 16;
          break;
        }
        case 17: {
          ret = 17;
          break;
        }
        case 18: {
          ret = 18;
          break;
        }
        case 19: {
          ret = 19;
        }
        default: {
          ret = -1;
          break;
        }
      }
      return ret|0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(7, wasm.main(4));
  assertEquals(16, wasm.main(10));
  assertEquals(-1, wasm.main(19));
  assertEquals(-1, wasm.main(23));
})();

(function TestSwitchHybridWithNoDefault() {
  function asmModule() {
    "use asm";
    function main(x) {
      x = x|0;
      var ret = 19;
      switch(x|0) {
        case 1: {
          ret = 1;
          break;
        }
        case 2: {
          ret = 2;
          break;
        }
        case 3: {
          ret = 3;
          break;
        }
        case 4: {
          ret = 4;
          break;
        }
        case 7: {
          ret = 7;
          break;
        }
      }
      return ret|0;
    }
    return {
      main: main,
    };
  }
  var wasm = asmModule();
  assertValidAsm(asmModule);
  assertEquals(2, wasm.main(2));
  assertEquals(7, wasm.main(7));
  assertEquals(19, wasm.main(-1));
})();

(function TestLargeSwitch() {
  function LargeSwitchGenerator(begin, end, gap, handle_case) {
    var str = "function asmModule() {\
      \"use asm\";\
      function main(x) {\
        x = x|0;\
        switch(x|0) {";
    for (var i = begin; i <= end; i = i + gap) {
      str = str.concat("case ", i.toString(), ": ", handle_case(i));
    }
    str = str.concat("default: return -1;\
        }\
        return -2;\
      }\
      return {main: main}; }");

    var decl = eval('(' + str + ')');
    var wasm = decl();
    assertValidAsm(decl);
    return wasm;
  }

  var handle_case = function(k) {
    return "return ".concat(k, ";");
  }
  var wasm = LargeSwitchGenerator(0, 513, 1, handle_case);
  for (var i = 0; i <= 513; i++) {
    assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));

  wasm = LargeSwitchGenerator(0, 1024, 3, handle_case);
  for (var i = 0; i <= 1024; i = i + 3) {
    assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));

  wasm = LargeSwitchGenerator(-2147483648, -2147483000, 1, handle_case);
  for (var i = -2147483648; i <= -2147483000; i++) {
    assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));
  assertEquals(-1, wasm.main(214748647));

  wasm = LargeSwitchGenerator(-2147483648, -2147483000, 3, handle_case);
  for (var i = -2147483648; i <= -2147483000; i = i + 3) {
    assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));
  assertEquals(-1, wasm.main(214748647));

  wasm = LargeSwitchGenerator(2147483000, 2147483647, 1, handle_case);
  for (var i = 2147483000; i <= 2147483647; i++) {
    assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));
  assertEquals(-1, wasm.main(-214748647));

  wasm = LargeSwitchGenerator(2147483000, 2147483647, 4, handle_case);
  for (var i = 2147483000; i <= 2147483647; i = i + 4) {
    assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));
  assertEquals(-1, wasm.main(-214748647));

  handle_case = function(k) {
    if (k != 7) return "return ".concat(k, ";");
    else return "break;";
  }
  wasm = LargeSwitchGenerator(0, 1499, 7, handle_case);
  for (var i = 0; i <= 1499; i = i + 7) {
    if (i == 7) assertEquals(-2, wasm.main(i));
    else assertEquals(i, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));

  handle_case = function(k) {
    if (k != 56) return "break;";
    else return "return 23;";
  }
  wasm = LargeSwitchGenerator(0, 638, 2, handle_case);
  for (var i = 0; i <= 638; i = i + 2) {
    if (i == 56) assertEquals(23, wasm.main(i));
    else assertEquals(-2, wasm.main(i));
  }
  assertEquals(-1, wasm.main(-1));
})();
