// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function WrapInAsmModule(func) {
  function MODULE_NAME(stdlib) {
    "use asm";
    var fround = stdlib.Math.fround;
    var Math_ceil = stdlib.Math.ceil;
    var Math_floor = stdlib.Math.floor;
    var Math_sqrt = stdlib.Math.sqrt;
    var Math_abs = stdlib.Math.abs;
    var Math_min = stdlib.Math.min;
    var Math_max = stdlib.Math.max;

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

var fround = Math.fround;
var Math_ceil = Math.ceil;
var Math_floor = Math.floor;
var Math_sqrt = Math.sqrt;
var Math_abs = Math.abs;
var Math_min = Math.min;
var Math_max = Math.max;

function f32_add(a, b) {
  a = fround(a);
  b = fround(b);
  return fround(fround(a) + fround(b));
}

function f32_sub(a, b) {
  a = fround(a);
  b = fround(b);
  return fround(fround(a) - fround(b));
}

function f32_mul(a, b) {
  a = fround(a);
  b = fround(b);
  return fround(fround(a) * fround(b));
}

function f32_div(a, b) {
  a = fround(a);
  b = fround(b);
  return fround(fround(a) / fround(b));
}

function f32_ceil(a) {
  a = fround(a);
  return fround(Math_ceil(fround(a)));
}

function f32_floor(a) {
  a = fround(a);
  return fround(Math_floor(fround(a)));
}

function f32_sqrt(a) {
  a = fround(a);
  return fround(Math_sqrt(fround(a)));
}

function f32_abs(a) {
  a = fround(a);
  return fround(Math_abs(fround(a)));
}

function f32_min(a, b) {
  a = fround(a);
  b = fround(b);
  return fround(Math_min(fround(a), fround(b)));
}

function f32_max(a, b) {
  a = fround(a);
  b = fround(b);
  return fround(Math_max(fround(a), fround(b)));
}

function f32_eq(a, b) {
  a = fround(a);
  b = fround(b);
  if (fround(a) == fround(b)) {
    return 1;
  }
  return 0;
}

function f32_ne(a, b) {
  a = fround(a);
  b = fround(b);
  if (fround(a) != fround(b)) {
    return 1;
  }
  return 0;
}

function f32_lt(a, b) {
  a = fround(a);
  b = fround(b);
  if (fround(a) < fround(b)) {
    return 1;
  }
  return 0;
}

function f32_lteq(a, b) {
  a = fround(a);
  b = fround(b);
  if (fround(a) <= fround(b)) {
    return 1;
  }
  return 0;
}

function f32_gt(a, b) {
  a = fround(a);
  b = fround(b);
  if (fround(a) > fround(b)) {
    return 1;
  }
  return 0;
}

function f32_gteq(a, b) {
  a = fround(a);
  b = fround(b);
  if (fround(a) >= fround(b)) {
    return 1;
  }
  return 0;
}

function f32_neg(a) {
  a = fround(a);
  return fround(-a);
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
  f32_add, f32_sub, f32_mul, f32_div, f32_ceil, f32_floor, f32_sqrt, f32_abs,
  f32_min, f32_max, f32_eq, f32_ne, f32_lt, f32_lteq, f32_gt, f32_gteq, f32_neg
];

(function () {
  for (func of funcs) {
    RunAsmJsTest(WrapInAsmModule(func), function (module) {
      if (func.length == 1) {
        for (a of inputs) {
          assertEquals(func(a), module.main(a));
          assertEquals(func(a / 11), module.main(a / 11));
          assertEquals(func(a / 430.9), module.main(a / 430.9));
          assertEquals(func(a / -31.1), module.main(a / -31.1));
        }
      } else {
        for (a of inputs) {
          for (b of inputs) {
            assertEquals(func(a, b), module.main(a, b));
            assertEquals(func(a / 11,  b), module.main(a / 11, b));
            assertEquals(func(a, b / 420.9), module.main(a, b / 420.9));
            assertEquals(func(a / -31.1, b), module.main(a / -31.1, b));
          }
        }
      }
    });
  }

})();
