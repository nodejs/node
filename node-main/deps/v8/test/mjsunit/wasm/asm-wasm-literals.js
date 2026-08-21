// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

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

function PositiveIntLiterals() {
  "use asm";
  function f0() { return 0; }
  function f1() { return 1; }
  function f4() { return 4; }
  function f64() { return 64; }
  function f127() { return 127; }
  function f128() { return 128; }
  function f256() { return 256; }
  function f1000() { return 1000; }
  function f2000000() { return 2000000; }
  function fmax() { return 2147483647; }
  return {f0: f0, f1: f1, f4: f4, f64: f64, f127: f127, f128: f128,
          f256: f256, f1000: f1000, f2000000: f2000000, fmax: fmax};
}

RunAsmJsTest(PositiveIntLiterals, function(module) {
  assertEquals(0, module.f0());
  assertEquals(1, module.f1());
  assertEquals(1, module.f1());
  assertEquals(4, module.f4());
  assertEquals(64, module.f64());
  assertEquals(128, module.f128());
  assertEquals(256, module.f256());
  assertEquals(1000, module.f1000());
  assertEquals(2000000, module.f2000000());
  assertEquals(2147483647, module.fmax());
});

function NegativeIntLiterals() {
  "use asm";
  function f1() { return -1; }
  function f4() { return -4; }
  function f64() { return -64; }
  function f127() { return -127; }
  function f128() { return -128; }
  function f256() { return -256; }
  function f1000() { return -1000; }
  function f2000000() { return -2000000; }
  function fmin() { return -2147483648; }
  return {f1: f1, f4: f4, f64: f64, f127: f127, f128: f128,
          f256: f256, f1000: f1000, f2000000: f2000000, fmin: fmin};
}

RunAsmJsTest(NegativeIntLiterals, function (module) {
  assertEquals(-1, module.f1());
  assertEquals(-4, module.f4());
  assertEquals(-64, module.f64());
  assertEquals(-127, module.f127());
  assertEquals(-128, module.f128());
  assertEquals(-256, module.f256());
  assertEquals(-1000, module.f1000());
  assertEquals(-2000000, module.f2000000());
  assertEquals(-2147483648, module.fmin());
});

function PositiveUnsignedLiterals() {
  "use asm";
  function f0() { return +(0 >>> 0); }
  function f1() { return +(1 >>> 0); }
  function f4() { return +(4 >>> 0); }
  function f64() { return +(64 >>> 0); }
  function f127() { return +(127 >>> 0); }
  function f128() { return +(128 >>> 0); }
  function f256() { return +(256 >>> 0); }
  function f1000() { return +(1000 >>> 0); }
  function f2000000() { return +(2000000 >>> 0); }
  function fmax() { return +(2147483647 >>> 0); }
  return {f0: f0, f1: f1, f4: f4, f64: f64, f127: f127, f128: f128,
          f256: f256, f1000: f1000, f2000000: f2000000, fmax: fmax};
}

RunAsmJsTest(PositiveUnsignedLiterals, function (module) {
  assertEquals(0, module.f0());
  assertEquals(1, module.f1());
  assertEquals(4, module.f4());
  assertEquals(64, module.f64());
  assertEquals(128, module.f128());
  assertEquals(256, module.f256());
  assertEquals(1000, module.f1000());
  assertEquals(2000000, module.f2000000());
  assertEquals(2147483647, module.fmax());
});

function LargeUnsignedLiterals() {
  "use asm";
  function a() {
    var x = 2147483648;
    return +(x >>> 0);
  }
  function b() {
    var x = 2147483649;
    return +(x >>> 0);
  }
  function c() {
    var x = 0x80000000;
    return +(x >>> 0);
  }
  function d() {
    var x = 0x80000001;
    return +(x >>> 0);
  }
  function e() {
    var x = 0xffffffff;
    return +(x >>> 0);
  }
  return {a: a, b: b, c: c, d: d, e: e};
}

RunAsmJsTest(LargeUnsignedLiterals, function(module) {
  assertEquals(2147483648, module.a());
  assertEquals(2147483649, module.b());
  assertEquals(0x80000000, module.c());
  assertEquals(0x80000001, module.d());
  assertEquals(0xffffffff, module.e());
});

function ManyI32() {
  "use asm";
  function main() {
    var a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0, i = 0, j = 0;
    a =          1 +          -2 +          3 +          -4 | 0;
    b =         11 +         -22 +         33 +         -44 | 0;
    c =        111 +        -222 +        333 +        -444 | 0;
    d =       1111 +       -2222 +       3333 +       -4444 | 0;
    e =      11111 +      -22222 +      33333 +      -44444 | 0;
    f =     155555 +     -266666 +     377777 +     -488888 | 0;
    g =    1155555 +    -2266666 +    3377777 +    -4488888 | 0;
    h =   11155555 +   -22266666 +   33377777 +   -44488888 | 0;
    i =  111155555 +  -222266666 +  333377777 +  -444488888 | 0;
    j = (
      0x1        + 0x2        + 0x4        + 0x8       +
      0x10       + 0x20       + 0x40       + 0x80      +
      0x10F      + 0x200      + 0x400      + 0x800     +
      0x10E0     + 0x20F0     + 0x4000     + 0x8000    +
      0x10D00    + 0x20E00    + 0x400F0    + 0x80002   +
      0x10C000   + 0x20D000   + 0x400E00   + 0x800030  +
      0x10B0000  + 0x20C0000  + 0x400D000  + 0x8000400 +
      0x10A00000 + 0x20B00000 + 0x400C0000 + 0x80005000
    ) | 0;
    return (a + b + c + d + e + f + g + h + i + j) | 0;
  }
  return {main: main};
}

RunAsmJsTest(ManyI32, function(module) {
  assertEquals(-222411306, module.main());
});


function ManyF64a() {
  "use asm";
  function main() {
    var a = 0.0, b = 0.0, c = 0.0, d = 0.0,
        e = 0.0, f = 0.0, g = 0.0, h = 0.0, i = 0.0;
    a = +(       0.1 +         -0.2 +         0.3 +         -0.4);
    b = +(       1.1 +         -2.2 +        0.33 +         -4.4);
    c = +(      11.1 +        -22.2 +        3.33 +        -4.44);
    d = +(     111.1 +       -222.2 +       33.33 +       -4.444);
    e = +(    1111.1 +      -2222.2 +      333.33 +      -4.4444);
    f = +(   15555.5 +     -26666.6 +     3777.77 +     -4.88888);
    g = +(  115555.5 +    -226666.6 +    33777.77 +    -4.488888);
    h = +( 1115555.5 +   -2226666.6 +   333777.77 +   -4.4488888);
    i = +(11115555.5 +  -22226666.6 +  3333777.77 +  -4.44488888);
    return +(a + b + c + d + e + f + g + h + i);
  }
  return {main: main};
}

RunAsmJsTest(ManyF64a, function(module) {
  assertEquals(-8640233.599945681, module.main());
});

function ManyF64b() {
  "use asm";
  function k1() { return +(1.0e-25 + 3.0e-25 + 5.0e-25 + 6.0e-25 + 9.0e-25); }
  function k2() { return +(1.0e-20 + 3.0e-20 + 5.0e-20 + 6.0e-20 + 9.0e-20); }
  function k3() { return +(1.0e-15 + 3.0e-15 + 5.0e-15 + 6.0e-15 + 9.0e-15); }
  function k4() { return +(1.0e-10 + 3.0e-10 + 5.0e-10 + 6.0e-10 + 9.0e-10); }
  function k5() { return +(1.0e-5 + 3.0e-5 + 5.0e-5 + 6.0e-5 + 9.0e-5); }
  function k6() { return +(1.1e+0 + 3.1e+0 + 5.1e+0 + 6.1e+0 + 9.1e+0); }

  return {k1: k1, k2: k2, k3: k3, k4: k4, k5: k5, k6: k6};
}

RunAsmJsTest(ManyF64b, function(module) {
  assertEquals(2.4e-24, module.k1());
  assertEquals(2.4e-19, module.k2());
  assertEquals(2.4e-14, module.k3());
  assertEquals(2.4e-9, module.k4());
  assertEquals(0.00024000000000000003, module.k5());
  assertEquals(24.5, module.k6());
});


function ManyF64c() {
  "use asm";
  function k1() { return +(1.0e+25 + 3.0e+25 + 5.0e+25 + 6.0e+25 + 9.0e+25); }
  function k2() { return +(1.0e+20 + 3.0e+20 + 5.0e+20 + 6.0e+20 + 9.0e+20); }
  function k3() { return +(1.0e+15 + 3.0e+15 + 5.0e+15 + 6.0e+15 + 9.0e+15); }
  function k4() { return +(1.0e+10 + 3.0e+10 + 5.0e+10 + 6.0e+10 + 9.0e+10); }
  function k5() { return +(1.0e+5 + 3.0e+5 + 5.0e+5 + 6.0e+5 + 9.0e+5); }
  function k6() { return +(1.4e+0 + 3.4e+0 + 5.4e+0 + 6.4e+0 + 9.4e+0); }

  return {k1: k1, k2: k2, k3: k3, k4: k4, k5: k5, k6: k6};
}

RunAsmJsTest(ManyF64c, function(module) {
  assertEquals(2.4000000000000004e+26, module.k1());
  assertEquals(2.4e+21, module.k2());
  assertEquals(2.4e+16, module.k3());
  assertEquals(2.4e+11, module.k4());
  assertEquals(2.4e+6, module.k5());
  assertEquals(26, module.k6());
});

function ManyF32a(stdlib) {
  "use asm";
  var F = stdlib.Math.fround;

  function k1() { return F(F(1.0e-25) + F(5.0e-25) + F(6.0e-25) + F(9.0e-25)); }
  function k2() { return F(F(1.0e-20) + F(5.0e-20) + F(6.0e-20) + F(9.0e-20)); }
  function k3() { return F(F(1.0e-15) + F(5.0e-15) + F(6.0e-15) + F(9.0e-15)); }
  function k4() { return F(F(1.0e-10) + F(5.0e-10) + F(6.0e-10) + F(9.0e-10)); }
  function k5() { return F(F(1.0e-5)  + F(5.0e-5)  + F(6.0e-5)  + F(9.0e-5)); }
  function k6() { return F(F(1.1e+0)  + F(5.1e+0)  + F(6.1e+0)  + F(9.1e+0)); }

  return {k1: k1, k2: k2, k3: k3, k4: k4, k5: k5, k6: k6};
}

if (false) {
  // TODO(bradnelson): fails validation of F32 literals somehow.
RunAsmJsTest(ManyF32a, function(module) {
  assertEquals(2.0999999917333043e-24, module.k1());
  assertEquals(2.099999868734112e-19, module.k2());
  assertEquals(2.099999997029825e-14, module.k3());
  assertEquals(2.099999951710174e-9, module.k4());
  assertEquals(0.0002099999983329326, module.k5());
  assertEquals(21.399999618530273, module.k6());
});
}
