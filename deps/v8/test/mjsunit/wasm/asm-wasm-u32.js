// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function WrapInAsmModule(func) {
  function MODULE_NAME(stdlib) {
    "use asm";
    var imul = stdlib.Math.imul;

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

const imul = Math.imul;

function u32_add(a, b) {
  a = a | 0;
  b = b | 0;
  return +(((a >>> 0) + (b >>> 0)) >>> 0);
}

function u32_sub(a, b) {
  a = a | 0;
  b = b | 0;
  return +(((a >>> 0) - (b >>> 0)) >>> 0);
}

function u32_mul(a, b) {
  a = a | 0;
  b = b | 0;
  return +imul(a >>> 0, b >>> 0);
}

function u32_div(a, b) {
  a = a | 0;
  b = b | 0;
  return +(((a >>> 0) / (b >>> 0)) >>> 0);
}

function u32_mod(a, b) {
  a = a | 0;
  b = b | 0;
  return +(((a >>> 0) % (b >>> 0)) >>> 0);
}

function u32_and(a, b) {
  a = a | 0;
  b = b | 0;
  return +((a >>> 0) & (b >>> 0));
}

function u32_or(a, b) {
  a = a | 0;
  b = b | 0;
  return +((a >>> 0) | (b >>> 0));
}

function u32_xor(a, b) {
  a = a | 0;
  b = b | 0;
  return +((a >>> 0) ^ (b >>> 0));
}

function u32_shl(a, b) {
  a = a | 0;
  b = b | 0;
  return +((a >>> 0) << (b >>> 0));
}

function u32_shr(a, b) {
  a = a | 0;
  b = b | 0;
  return +((a >>> 0) >> (b >>> 0));
}

function u32_sar(a, b) {
  a = a | 0;
  b = b | 0;
  return ((a >>> 0) >>> (b >>> 0)) | 0;
}

function u32_eq(a, b) {
  a = a | 0;
  b = b | 0;
  if ((a >>> 0) == (b >>> 0)) {
    return 1;
  }
  return 0;
}

function u32_ne(a, b) {
  a = a | 0;
  b = b | 0;
  if ((a >>> 0) < (b >>> 0)) {
    return 1;
  }
  return 0;
}

function u32_lt(a, b) {
  a = a | 0;
  b = b | 0;
  if ((a >>> 0) < (b >>> 0)) {
    return 1;
  }
  return 0;
}

function u32_lteq(a, b) {
  a = a | 0;
  b = b | 0;
  if ((a >>> 0) <= (b >>> 0)) {
    return 1;
  }
  return 0;
}

function u32_gt(a, b) {
  a = a | 0;
  b = b | 0;
  if ((a >>> 0) > (b >>> 0)) {
    return 1;
  }
  return 0;
}

function u32_gteq(a, b) {
  a = a | 0;
  b = b | 0;
  if ((a >>> 0) >= (b >>> 0)) {
    return 1;
  }
  return 0;
}

function u32_neg(a) {
  a = a | 0;
  return (-a) | 0;
}

function u32_invert(a) {
  a = a | 0;
  return (~a) | 0;
}


var inputs = [
  0, 1, 2, 3, 4,
  10, 20, 30, 31, 32, 33, 100, 2000,
  30000, 400000, 5000000,
  100000000, 2000000000,
  2147483646,
  2147483647,
  2147483648,
  2147483649,
  0x273a798e, 0x187937a3, 0xece3af83, 0x5495a16b, 0x0b668ecc, 0x11223344,
  0x0000009e, 0x00000043, 0x0000af73, 0x0000116b, 0x00658ecc, 0x002b3b4c,
  0x88776655, 0x70000000, 0x07200000, 0x7fffffff, 0x56123761, 0x7fffff00,
  0x761c4761, 0x80000000, 0x88888888, 0xa0000000, 0xdddddddd, 0xe0000000,
  0xeeeeeeee, 0xfffffffd, 0xf0000000, 0x007fffff, 0x003fffff, 0x001fffff,
  0x000fffff, 0x0007ffff, 0x0003ffff, 0x0001ffff, 0x0000ffff, 0x00007fff,
  0x00003fff, 0x00001fff, 0x00000fff, 0x000007ff, 0x000003ff, 0x000001ff,
  -1, -2, -3, -4,
  -10, -20, -30, -31, -32, -33, -100, -2000,
  -30000, -400000, -5000000,
  -100000000, -2000000000,
  -2147483646,
  -2147483647,
  -2147483648,
  -2147483649,
];

var funcs = [
  u32_add,
  u32_sub,
  u32_div,
  u32_mod,
// TODO(titzer): u32_mul crashes turbofan in asm.js mode
  u32_and,
  u32_or,
  u32_xor,
  u32_shl,
  u32_shr,
  u32_sar,
  u32_eq,
  u32_ne,
  u32_lt,
  u32_lteq,
  u32_gt,
  u32_gteq,
  u32_neg,
  u32_invert,
  // TODO(titzer): u32_min
  // TODO(titzer): u32_max
  // TODO(titzer): u32_abs
];

(function () {
  for (func of funcs) {
    RunAsmJsTest(WrapInAsmModule(func), function (module) {
      for (a of inputs) {
        for (b of inputs) {
          var expected = func(a, b);
          assertEquals(expected, module.main(a, b));
        }
      }
    });
  }

})();
