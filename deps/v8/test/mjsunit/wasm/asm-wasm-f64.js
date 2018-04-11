// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function WrapInAsmModule(func) {
  function MODULE_NAME(stdlib) {
    "use asm";
    var Math_ceil = stdlib.Math.ceil;
    var Math_floor = stdlib.Math.floor;
    var Math_sqrt = stdlib.Math.sqrt;
    var Math_abs = stdlib.Math.abs;
    var Math_min = stdlib.Math.min;
    var Math_max = stdlib.Math.max;
    var Math_acos = stdlib.Math.acos;
    var Math_asin = stdlib.Math.asin;
    var Math_atan = stdlib.Math.atan;
    var Math_cos = stdlib.Math.cos;
    var Math_sin = stdlib.Math.sin;
    var Math_tan = stdlib.Math.tan;
    var Math_exp = stdlib.Math.exp;
    var Math_log = stdlib.Math.log;
    var Math_atan2 = stdlib.Math.atan2;

    FUNC_BODY
    return {main: FUNC_NAME};
  }

  var source = MODULE_NAME.toString()
    .replace(/MODULE_NAME/g, func.name + "_module")
    .replace(/FUNC_BODY/g, func.toString())
    .replace(/FUNC_NAME/g, func.name);
  return eval("(" + source + ")");
}

function RunAsmJsTest(asmfunc, expect) {
  var asm_source = asmfunc.toString();
  var nonasm_source = asm_source.replace(new RegExp("use asm"), "");
  var stdlib = {Math: Math};

  print("Testing " + asmfunc.name + " (js)...");
  var js_module = eval("(" + nonasm_source + ")")(stdlib);
  expect(js_module);

  print("Testing " + asmfunc.name + " (asm.js)...");
  var asm_module = asmfunc(stdlib);
  assertTrue(%IsAsmWasmCode(asmfunc));
  expect(asm_module);
}

const Math_ceil = Math.ceil;
const Math_floor = Math.floor;
const Math_sqrt = Math.sqrt;
const Math_abs = Math.abs;
const Math_min = Math.min;
const Math_max = Math.max;
const Math_acos = Math.acos;
const Math_asin = Math.asin;
const Math_atan = Math.atan;
const Math_cos = Math.cos;
const Math_sin = Math.sin;
const Math_tan = Math.tan;
const Math_exp = Math.exp;
const Math_log = Math.log;
const Math_atan2 = Math.atan2;

function f64_add(a, b) {
  a = +a;
  b = +b;
  return +(+a + +b);
}

function f64_sub(a, b) {
  a = +a;
  b = +b;
  return +(+a - +b);
}

function f64_mul(a, b) {
  a = +a;
  b = +b;
  return +(+a * +b);
}

function f64_div(a, b) {
  a = +a;
  b = +b;
  return +(+a / +b);
}

function f64_eq(a, b) {
  a = +a;
  b = +b;
  if (+a == +b) {
    return 1;
  }
  return 0;
}

function f64_ne(a, b) {
  a = +a;
  b = +b;
  if (+a != +b) {
    return 1;
  }
  return 0;
}

function f64_lt(a, b) {
  a = +a;
  b = +b;
  if (+a < +b) {
    return 1;
  }
  return 0;
}

function f64_lteq(a, b) {
  a = +a;
  b = +b;
  if (+a <= +b) {
    return 1;
  }
  return 0;
}

function f64_gt(a, b) {
  a = +a;
  b = +b;
  if (+a > +b) {
    return 1;
  }
  return 0;
}

function f64_gteq(a, b) {
  a = +a;
  b = +b;
  if (+a >= +b) {
    return 1;
  }
  return 0;
}

function f64_ceil(a) {
  a = +a;
  return +(Math_ceil(+a));
}

function f64_floor(a) {
  a = +a;
  return +(Math_floor(+a));
}

function f64_sqrt(a) {
  a = +a;
  return +(Math_sqrt(+a));
}

function f64_abs(a) {
  a = +a;
  return +(Math_abs(+a));
}

function f64_min(a, b) {
  a = +a;
  b = +b;
  return +(Math_min(+a, +b));
}

function f64_max(a, b) {
  a = +a;
  b = +b;
  return +(Math_max(+a, +b));
}

function f64_acos(a) {
  a = +a;
  return +Math_acos(+a);
}

function f64_asin(a) {
  a = +a;
  return +Math_asin(+a);
}

function f64_atan(a) {
  a = +a;
  return +Math_atan(+a);
}

function f64_cos(a) {
  a = +a;
  return +Math_cos(+a);
}

function f64_sin(a) {
  a = +a;
  return +Math_sin(+a);
}

function f64_tan(a) {
  a = +a;
  return +Math_tan(+a);
}

function f64_exp(a) {
  a = +a;
  return +Math_exp(+a);
}

function f64_log(a) {
  a = +a;
  return +Math_log(+a);
}

function f64_atan2(a, b) {
  a = +a;
  b = +b;
  return +Math_atan2(+a, +b);
}

function f64_neg(a) {
  a = +a;
  return +(-a);
}


var inputs = [
  0, 1, 2, 3, 4,
  NaN,
  Infinity,
  -Infinity,
  10, 20, 30, 31, 32, 33, 100, 2000,
  30000, 400000, 5000000,
  100000000, 2000000000,
  2147483646,
  2147483647,
  2147483648,
  2147483649,
  0x273a798e, 0x187937a3, 0xece3af83, 0x5495a16b, 0x0b668ecc, 0x11223344,
  0x0000af73, 0x0000116b, 0x00658ecc, 0x002b3b4c,
  0x88776655, 0x70000000, 0x07200000, 0x7fffffff, 0x56123761, 0x7fffff00,
  0xeeeeeeee, 0xfffffffd, 0xf0000000, 0x007fffff, 0x003fffff, 0x001fffff,
  -0,
  -1, -2, -3, -4,
  -10, -20, -30, -31, -32, -33, -100, -2000,
  -30000, -400000, -5000000,
  -100000000, -2000000000,
  -2147483646,
  -2147483647,
  -2147483648,
  -2147483649,
  0.1,
  1.1e-2,
  1.2e-4,
  1.3e-8,
  1.4e-11,
  1.5e-12,
  1.6e-13
];

var funcs = [
  f64_add,  f64_sub, f64_mul,  f64_div,  f64_eq,    f64_ne,   f64_lt,
  f64_lteq, f64_gt,  f64_gteq, f64_ceil, f64_floor, f64_sqrt, f64_abs,
  f64_neg,  f64_min, f64_max,  f64_acos, f64_asin,  f64_atan, f64_cos,
  f64_sin,  f64_tan, f64_exp,  f64_log,  f64_atan2,
];

(function () {
  for (func of funcs) {
    RunAsmJsTest(WrapInAsmModule(func), function (module) {
      if (func.length == 1) {
        for (a of inputs) {
          assertEquals(func(a), module.main(a));
          assertEquals(func(a / 10), module.main(a / 10));
          assertEquals(func(a / 440.9), module.main(a / 440.9));
          assertEquals(func(a / -33.1), module.main(a / -33.1));
        }
      } else {
        for (a of inputs) {
          for (b of inputs) {
            assertEquals(func(a, b), module.main(a, b));
            assertEquals(func(a / 10,  b), module.main(a / 10, b));
            assertEquals(func(a, b / 440.9), module.main(a, b / 440.9));
            assertEquals(func(a / -33.1, b), module.main(a / -33.1, b));
          }
        }
      }
    });
  }
})();
