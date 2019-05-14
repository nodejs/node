// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ----------------------------------------------------------------------
// toString
function testToString(a, b) {
  assertEquals(a, b.toString());
}

function testToStringP(a, b, c) {
  assertEquals(a, b.toString(c));
}

testToString("NaN", (NaN));
testToString("Infinity", (1/0));
testToString("-Infinity", (-1/0));
testToString("0", (0));
testToString("9", (9));
testToString("90", (90));
testToString("90.12", (90.12));
testToString("0.1", (0.1));
testToString("0.01", (0.01));
testToString("0.0123", (0.0123));
testToString("111111111111111110000", (111111111111111111111));
testToString("1.1111111111111111e+21", (1111111111111111111111));
testToString("1.1111111111111111e+22", (11111111111111111111111));
testToString("0.00001", (0.00001));
testToString("0.000001", (0.000001));
testToString("1e-7", (0.0000001));
testToString("1.2e-7", (0.00000012));
testToString("1.23e-7", (0.000000123));
testToString("1e-8", (0.00000001));
testToString("1.2e-8", (0.000000012));
testToString("1.23e-8", (0.0000000123));

testToString("0", (-0));
testToString("-9", (-9));
testToString("-90", (-90));
testToString("-90.12", (-90.12));
testToString("-0.1", (-0.1));
testToString("-0.01", (-0.01));
testToString("-0.0123", (-0.0123));
testToString("-111111111111111110000", (-111111111111111111111));
testToString("-1.1111111111111111e+21", (-1111111111111111111111));
testToString("-1.1111111111111111e+22", (-11111111111111111111111));
testToString("-0.00001", (-0.00001));
testToString("-0.000001", (-0.000001));
testToString("-1e-7", (-0.0000001));
testToString("-1.2e-7", (-0.00000012));
testToString("-1.23e-7", (-0.000000123));
testToString("-1e-8", (-0.00000001));
testToString("-1.2e-8", (-0.000000012));
testToString("-1.23e-8", (-0.0000000123));

testToString("1000", (1000));
testToString("0.00001", (0.00001));
testToString("1000000000000000100", (1000000000000000128));
testToString("1e+21", (1000000000000000012800));
testToString("-1e+21", (-1000000000000000012800));
testToString("1e-7", (0.0000001));
testToString("-1e-7", (-0.0000001));
testToString("1.0000000000000001e+21", (1000000000000000128000));
testToString("0.000001", (0.000001));
testToString("1e-7", (0.0000001));

testToStringP("NaN", (NaN), 16);
testToStringP("Infinity", (1/0), 16);
testToStringP("-Infinity", (-1/0), 16);
testToStringP("0", (0), 16);
testToStringP("9", (9), 16);
testToStringP("5a", (90), 16);
testToStringP("5a.1eb851eb852", (90.12), 16);
testToStringP("0.1999999999999a", (0.1), 16);
testToStringP("0.028f5c28f5c28f6", (0.01), 16);
testToStringP("0.032617c1bda511a", (0.0123), 16);
testToStringP("605f9f6dd18bc8000", (111111111111111111111), 16);
testToStringP("3c3bc3a4a2f75c0000", (1111111111111111111111), 16);
testToStringP("25a55a46e5da9a00000", (11111111111111111111111), 16);
testToStringP("0.0000a7c5ac471b4788", (0.00001), 16);
testToStringP("0.000010c6f7a0b5ed8d", (0.000001), 16);
testToStringP("0.000001ad7f29abcaf48", (0.0000001), 16);
testToStringP("0.000002036565348d256", (0.00000012), 16);
testToStringP("0.0000021047ee22aa466", (0.000000123), 16);
testToStringP("0.0000002af31dc4611874", (0.00000001), 16);
testToStringP("0.000000338a23b87483be", (0.000000012), 16);
testToStringP("0.00000034d3fe36aaa0a2", (0.0000000123), 16);

testToStringP("0", (-0), 16);
testToStringP("-9", (-9), 16);
testToStringP("-5a", (-90), 16);
testToStringP("-5a.1eb851eb852", (-90.12), 16);
testToStringP("-0.1999999999999a", (-0.1), 16);
testToStringP("-0.028f5c28f5c28f6", (-0.01), 16);
testToStringP("-0.032617c1bda511a", (-0.0123), 16);
testToStringP("-605f9f6dd18bc8000", (-111111111111111111111), 16);
testToStringP("-3c3bc3a4a2f75c0000", (-1111111111111111111111), 16);
testToStringP("-25a55a46e5da9a00000", (-11111111111111111111111), 16);
testToStringP("-0.0000a7c5ac471b4788", (-0.00001), 16);
testToStringP("-0.000010c6f7a0b5ed8d", (-0.000001), 16);
testToStringP("-0.000001ad7f29abcaf48", (-0.0000001), 16);
testToStringP("-0.000002036565348d256", (-0.00000012), 16);
testToStringP("-0.0000021047ee22aa466", (-0.000000123), 16);
testToStringP("-0.0000002af31dc4611874", (-0.00000001), 16);
testToStringP("-0.000000338a23b87483be", (-0.000000012), 16);
testToStringP("-0.00000034d3fe36aaa0a2", (-0.0000000123), 16);

testToString("4294967296", Math.pow(2,32));
testToStringP("ffffffff", (Math.pow(2,32)-1), 16);
testToStringP("11111111111111111111111111111111", (Math.pow(2,32)-1), 2);
testToStringP("5yc1z", (10000007), 36);
testToStringP("0", (0), 36);
testToStringP("0", (0), 16);
testToStringP("0", (0), 10);
testToStringP("0", (0), 8);
testToStringP("0", (0), 2);
testToStringP("100000000000000000000000000000000", Math.pow(2,32), 2);
testToStringP("100000000000000000000000000000001", (Math.pow(2,32) + 1), 2);
testToStringP("100000000000080", (0x100000000000081), 16);
testToStringP("1000000000000100", (-(-'0x1000000000000081')), 16);
testToStringP("1000000000000000", (-(-'0x1000000000000080')), 16);
testToStringP("1000000000000000", (-(-'0x100000000000007F')), 16);
testToStringP("100000000000000000000000000000000000000000000000010000000", (0x100000000000081), 2);
testToStringP("-11111111111111111111111111111111", (-(Math.pow(2,32)-1)), 2);
testToStringP("-5yc1z", (-10000007), 36);
testToStringP("-100000000000000000000000000000000", (-Math.pow(2,32)), 2);
testToStringP("-100000000000000000000000000000001", (-(Math.pow(2,32) + 1)), 2);
testToStringP("-100000000000080", (-0x100000000000081), 16);
testToStringP("-100000000000000000000000000000000000000000000000010000000", (-0x100000000000081), 2);
testToStringP("8.8", (8.5), 16);
testToStringP("-8.8", (-8.5), 16);

// ----------------------------------------------------------------------
// toFixed
function testToFixed(a, b, c) {
  assertEquals(a, b.toFixed(c));
}

testToFixed("NaN", (NaN), (2));
testToFixed("Infinity", (1/0), (2));
testToFixed("-Infinity", (-1/0), (2));

testToFixed("1.1111111111111111e+21", (1111111111111111111111), (8));
testToFixed("0.1", (0.1), (1));
testToFixed("0.10", (0.1), (2));
testToFixed("0.100", (0.1), (3));
testToFixed("0.01", (0.01), (2));
testToFixed("0.010", (0.01), (3));
testToFixed("0.0100", (0.01), (4));
testToFixed("0.00", (0.001), (2));
testToFixed("0.001", (0.001), (3));
testToFixed("0.0010", (0.001), (4));
testToFixed("1.0000", (1), (4));
testToFixed("1.0", (1), (1));
testToFixed("1", (1), (0));
testToFixed("12", (12), (0));
testToFixed("1", (1.1), (0));
testToFixed("12", (12.1), (0));
testToFixed("1", (1.12), (0));
testToFixed("12", (12.12), (0));
testToFixed("0.0000006", (0.0000006), (7));
testToFixed("0.00000006", (0.00000006), (8));
testToFixed("0.000000060", (0.00000006), (9));
testToFixed("0.0000000600", (0.00000006), (10));
testToFixed("0", (0), (0));
testToFixed("0.0", (0), (1));
testToFixed("0.00", (0), (2));

testToFixed("-1.1111111111111111e+21", (-1111111111111111111111), (8));
testToFixed("-0.1", (-0.1), (1));
testToFixed("-0.10", (-0.1), (2));
testToFixed("-0.100", (-0.1), (3));
testToFixed("-0.01", (-0.01), (2));
testToFixed("-0.010", (-0.01), (3));
testToFixed("-0.0100", (-0.01), (4));
testToFixed("-0.00", (-0.001), (2));
testToFixed("-0.001", (-0.001), (3));
testToFixed("-0.0010", (-0.001), (4));
testToFixed("-1.0000", (-1), (4));
testToFixed("-1.0", (-1), (1));
testToFixed("-1", (-1), (0));
testToFixed("-1", (-1.1), (0));
testToFixed("-12", (-12.1), (0));
testToFixed("-1", (-1.12), (0));
testToFixed("-12", (-12.12), (0));
testToFixed("-0.0000006", (-0.0000006), (7));
testToFixed("-0.00000006", (-0.00000006), (8));
testToFixed("-0.000000060", (-0.00000006), (9));
testToFixed("-0.0000000600", (-0.00000006), (10));
testToFixed("0", (-0), (0));
testToFixed("0.0", (-0), (1));
testToFixed("0.00", (-0), (2));

testToFixed("0.00001", (0.00001), (5));
testToFixed("0.00000000000000000010", (0.0000000000000000001), (20));
testToFixed("0.00001000000000000", (0.00001), (17));
testToFixed("1.00000000000000000", (1), (17));
testToFixed("100000000000000128.0", (100000000000000128), (1));
testToFixed("10000000000000128.00", (10000000000000128), (2));
testToFixed("10000000000000128.00000000000000000000", (10000000000000128), (20));
testToFixed("-42.000", (-42), (3));
testToFixed("-0.00000000000000000010", (-0.0000000000000000001), (20));
testToFixed("0.12312312312312299889", (0.123123123123123), (20));

assertEquals("-1000000000000000128", (-1000000000000000128).toFixed());
assertEquals("0", (0).toFixed());
assertEquals("1000000000000000128", (1000000000000000128).toFixed());
assertEquals("1000", (1000).toFixed());
assertEquals("0", (0.00001).toFixed());
// Test that we round up even when the last digit generated is even.
// dtoa does not do this in its original form.
assertEquals("1", 0.5.toFixed(0), "0.5.toFixed(0)");
assertEquals("-1", (-0.5).toFixed(0), "(-0.5).toFixed(0)");
assertEquals("1.3", 1.25.toFixed(1), "1.25.toFixed(1)");
// This is bizare, but Spidermonkey and KJS behave the same.
assertEquals("234.2040", (234.20405).toFixed(4), "234.2040.toFixed(4)");
assertEquals("234.2041", (234.2040506).toFixed(4));

// ----------------------------------------------------------------------
// toExponential
function testToExponential(a, b) {
  assertEquals(a, b.toExponential());
}

function testToExponentialP(a, b, c) {
  assertEquals(a, b.toExponential(c));
}

testToExponential("1e+0", (1));
testToExponential("1.1e+1", (11));
testToExponential("1.12e+2", (112));
testToExponential("1e-1", (0.1));
testToExponential("1.1e-1", (0.11));
testToExponential("1.12e-1", (0.112));
testToExponential("-1e+0", (-1));
testToExponential("-1.1e+1", (-11));
testToExponential("-1.12e+2", (-112));
testToExponential("-1e-1", (-0.1));
testToExponential("-1.1e-1", (-0.11));
testToExponential("-1.12e-1", (-0.112));
testToExponential("0e+0", (0));
testToExponential("1.12356e-4", (0.000112356));
testToExponential("-1.12356e-4", (-0.000112356));

testToExponentialP("1e+0", (1), (0));
testToExponentialP("1e+1", (11), (0));
testToExponentialP("1e+2", (112), (0));
testToExponentialP("1.0e+0", (1), (1));
testToExponentialP("1.1e+1", (11), (1));
testToExponentialP("1.1e+2", (112), (1));
testToExponentialP("1.00e+0", (1), (2));
testToExponentialP("1.10e+1", (11), (2));
testToExponentialP("1.12e+2", (112), (2));
testToExponentialP("1.000e+0", (1), (3));
testToExponentialP("1.100e+1", (11), (3));
testToExponentialP("1.120e+2", (112), (3));
testToExponentialP("1e-1", (0.1), (0));
testToExponentialP("1e-1", (0.11), (0));
testToExponentialP("1e-1", (0.112), (0));
testToExponentialP("1.0e-1", (0.1), (1));
testToExponentialP("1.1e-1", (0.11), (1));
testToExponentialP("1.1e-1", (0.112), (1));
testToExponentialP("1.00e-1", (0.1), (2));
testToExponentialP("1.10e-1", (0.11), (2));
testToExponentialP("1.12e-1", (0.112), (2));
testToExponentialP("1.000e-1", (0.1), (3));
testToExponentialP("1.100e-1", (0.11), (3));
testToExponentialP("1.120e-1", (0.112), (3));

testToExponentialP("-1e+0", (-1), (0));
testToExponentialP("-1e+1", (-11), (0));
testToExponentialP("-1e+2", (-112), (0));
testToExponentialP("-1.0e+0", (-1), (1));
testToExponentialP("-1.1e+1", (-11), (1));
testToExponentialP("-1.1e+2", (-112), (1));
testToExponentialP("-1.00e+0", (-1), (2));
testToExponentialP("-1.10e+1", (-11), (2));
testToExponentialP("-1.12e+2", (-112), (2));
testToExponentialP("-1.000e+0", (-1), (3));
testToExponentialP("-1.100e+1", (-11), (3));
testToExponentialP("-1.120e+2", (-112), (3));
testToExponentialP("-1e-1", (-0.1), (0));
testToExponentialP("-1e-1", (-0.11), (0));
testToExponentialP("-1e-1", (-0.112), (0));
testToExponentialP("-1.0e-1", (-0.1), (1));
testToExponentialP("-1.1e-1", (-0.11), (1));
testToExponentialP("-1.1e-1", (-0.112), (1));
testToExponentialP("-1.00e-1", (-0.1), (2));
testToExponentialP("-1.10e-1", (-0.11), (2));
testToExponentialP("-1.12e-1", (-0.112), (2));
testToExponentialP("-1.000e-1", (-0.1), (3));
testToExponentialP("-1.100e-1", (-0.11), (3));
testToExponentialP("-1.120e-1", (-0.112), (3));

testToExponentialP("NaN", (NaN), (2));
testToExponentialP("Infinity", (Infinity), (2));
testToExponentialP("-Infinity", (-Infinity), (2));
testToExponentialP("1e+0", (1), (0));
testToExponentialP("0.00e+0", (0), (2));
testToExponentialP("1e+1", (11.2356), (0));
testToExponentialP("1.1236e+1", (11.2356), (4));
testToExponentialP("1.1236e-4", (0.000112356), (4));
testToExponentialP("-1.1236e-4", (-0.000112356), (4));

// ----------------------------------------------------------------------
// toPrecision
function testToPrecision(a, b, c) {
  assertEquals(a, b.toPrecision(c));
}

testToPrecision("NaN", (NaN), (1));
testToPrecision("Infinity", (Infinity), (2));
testToPrecision("-Infinity", (-Infinity), (2));
testToPrecision("0.000555000000000000", (0.000555), (15));
testToPrecision("5.55000000000000e-7", (0.000000555), (15));
testToPrecision("-5.55000000000000e-7", (-0.000000555), (15));
testToPrecision("1e+8", (123456789), (1));
testToPrecision("123456789", (123456789), (9));
testToPrecision("1.2345679e+8", (123456789), (8));
testToPrecision("1.234568e+8", (123456789), (7));
testToPrecision("-1.234568e+8", (-123456789), (7));
testToPrecision("-1.2e-9", Number(-.0000000012345), (2));
testToPrecision("-1.2e-8", Number(-.000000012345), (2));
testToPrecision("-1.2e-7", Number(-.00000012345), (2));
testToPrecision("-0.0000012", Number(-.0000012345), (2));
testToPrecision("-0.000012", Number(-.000012345), (2));
testToPrecision("-0.00012", Number(-.00012345), (2));
testToPrecision("-0.0012", Number(-.0012345), (2));
testToPrecision("-0.012", Number(-.012345), (2));
testToPrecision("-0.12", Number(-.12345), (2));
testToPrecision("-1.2", Number(-1.2345), (2));
testToPrecision("-12", Number(-12.345), (2));
testToPrecision("-1.2e+2", Number(-123.45), (2));
testToPrecision("-1.2e+3", Number(-1234.5), (2));
testToPrecision("-1.2e+4", Number(-12345), (2));
testToPrecision("-1.235e+4", Number(-12345.67), (4));
testToPrecision("-1.234e+4", Number(-12344.67), (4));
// Test that we round up even when the last digit generated is even.
// dtoa does not do this in its original form.
assertEquals("1.3", 1.25.toPrecision(2), "1.25.toPrecision(2)");
assertEquals("1.4", 1.35.toPrecision(2), "1.35.toPrecision(2)");
