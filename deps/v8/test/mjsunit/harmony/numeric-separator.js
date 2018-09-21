// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-numeric-separator

{
  const basic = 1_0_0_0;
  assertEquals(basic, 1000);
}
{
  const exponent = 1_0e+1;
  assertEquals(exponent, 10e+1);
}
{
  const exponent2 = 1_0e+1_0;
  assertEquals(exponent2, 10e+10);
}
{
  const hex = 0xF_F_FF;
  assertEquals(hex, 0xFFFF);
}
{
  const octal = 0o7_7_7;
  assertEquals(octal, 0o777);
}
{
  let exception = false;
  try {
    const code = `"use strict" const implicitOctal = 07_7_7`;
    eval(code);
  } catch(e) {
    exception = true;
    assertInstanceof(e, SyntaxError);
  }
  assertTrue(exception);
}

{
  const binary = 0b0_1_0_1_0;
  assertEquals(binary, 0b01010);
}
{
  const leadingZeros = 09_1_3;
  assertEquals(leadingZeros, 0913);
}

{
  const dot1 = 9_1.1_3;
  assertEquals(dot1, 91.13);

  const dot2 = 1.1_3;
  assertEquals(dot2, 1.13);

  const dot3 = 1_1.21;
  assertEquals(dot3, 11.21);
}

{
  const basic = Number('1_2_3');
  assertEquals(NaN, basic);
  const exponent = Number('1_0e+1');
  assertEquals(NaN, exponent);
  const exponent2 = Number('1_0e+1_0');
  assertEquals(NaN, exponent2);
  const hex = Number('0xF_F_FF');
  assertEquals(NaN, hex);
  const octal = Number('0o7_7_7');
  assertEquals(NaN, octal);
  const binary = Number('0b0_1_0_1_0');
  assertEquals(NaN, binary);
  const leadingZeros = Number('09_1_3');
  assertEquals(NaN, leadingZeros);
  const dot1 = Number('9_1.1_3');
  assertEquals(NaN, dot1);
  const dot2 = Number('1.1_3');
  assertEquals(NaN, dot2);
  const dot3 = Number('1_1.21');
  assertEquals(NaN, dot3);
}

{
  assertEquals(1, parseInt('1_2_3'));
  assertEquals(0, parseInt('0_1_0_1_0'));
  assertEquals(15, parseInt('0xF_F'));
  assertEquals(10, parseInt('10e+1_0'));
  assertEquals(0, parseInt('0o7_7_7'));
  assertEquals(0, parseInt('0b1_0_1_0'));
  assertEquals(9, parseInt('9_1.1_3'));
  assertEquals(1, parseInt('1.1_3'));
  assertEquals(1, parseInt('1_1.21'));
  assertEquals(17, parseInt('017_123'));

  assertEquals(1, parseInt('1_2_3', 10));
  assertEquals(15, parseInt('0xF_F', 16));
  assertEquals(7, parseInt('7_7_7', 8));
  assertEquals(1, parseInt('1_0_1_0', 2));
}

assertThrows('1_0_0_0_', SyntaxError);
assertThrows('1e_1', SyntaxError);
assertThrows('1e+_1', SyntaxError);
assertThrows('1_e+1', SyntaxError);
assertThrows('1__0', SyntaxError);
assertThrows('0x_1', SyntaxError);
assertThrows('0x1__1', SyntaxError);
assertThrows('0x1_', SyntaxError);
assertThrows('0b_0101', SyntaxError);
assertThrows('0b11_', SyntaxError);
assertThrows('0b1__1', SyntaxError);
assertThrows('0o777_', SyntaxError);
assertThrows('0o_777', SyntaxError);
assertThrows('0o7__77', SyntaxError);
assertThrows('0777_', SyntaxError);
assertThrows('07__77', SyntaxError);
assertThrows('07_7_7', SyntaxError);
