// Copyright 2008 the V8 project authors. All rights reserved.
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

function toNumber(val) {
  return Number(val);
}

// assertEquals(, toNumber());


assertEquals(123, toNumber(" 123"));
assertEquals(123, toNumber("\n123"));
assertEquals(123, toNumber("\r123"));
assertEquals(123, toNumber("\t123"));
assertEquals(123, toNumber("\f123"));

assertEquals(123, toNumber("123 "));
assertEquals(123, toNumber("123\n"));
assertEquals(123, toNumber("123\r"));
assertEquals(123, toNumber("123\t"));
assertEquals(123, toNumber("123\f"));

assertEquals(123, toNumber(" 123 "));
assertEquals(123, toNumber("\n123\n"));
assertEquals(123, toNumber("\r123\r"));
assertEquals(123, toNumber("\t123\t"));
assertEquals(123, toNumber("\f123\f"));

assertTrue(isNaN(toNumber(" NaN ")));
assertEquals(Infinity,  toNumber(" Infinity ") ," Infinity");
assertEquals(-Infinity, toNumber(" -Infinity "));
assertEquals(Infinity,  toNumber(" +Infinity "), " +Infinity");
assertEquals(Infinity,  toNumber("Infinity ") ,"Infinity");
assertEquals(-Infinity, toNumber("-Infinity "));
assertEquals(Infinity,  toNumber("+Infinity "), "+Infinity");

assertEquals(0,  toNumber("0"));
assertEquals(0,  toNumber("+0"));
assertEquals(-0, toNumber("-0"));

assertEquals(1,  toNumber("1"));
assertEquals(1,  toNumber("+1"));
assertEquals(-1, toNumber("-1"));

assertEquals(2,  toNumber("2"));
assertEquals(2,  toNumber("+2"));
assertEquals(-2, toNumber("-2"));

assertEquals(3.1415926,  toNumber("3.1415926"));
assertEquals(3.1415926,  toNumber("+3.1415926"));
assertEquals(-3.1415926, toNumber("-3.1415926"));

assertEquals(5,  toNumber("5."));
assertEquals(5,  toNumber("+5."));
assertEquals(-5, toNumber("-5."));

assertEquals(500,   toNumber("5e2"));
assertEquals(500,   toNumber("+5e2"));
assertEquals(-500,  toNumber("-5e2"));
assertEquals(500,   toNumber("5e+2"));
assertEquals(500,   toNumber("+5e+2"));
assertEquals(-500,  toNumber("-5e+2"));
assertEquals(0.05,  toNumber("5e-2"));
assertEquals(0.05,  toNumber("+5e-2"));
assertEquals(-0.05, toNumber("-5e-2"));

assertEquals(0.00001,   toNumber(".00001"));
assertEquals(0.00001,   toNumber("+.00001"));
assertEquals(-0.00001,  toNumber("-.00001"));
assertEquals(1,         toNumber(".00001e5"));
assertEquals(1,         toNumber("+.00001e5"));
assertEquals(-1,        toNumber("-.00001e5"));
assertEquals(1,         toNumber(".00001e+5"));
assertEquals(1,         toNumber("+.00001e+5"));
assertEquals(-1,        toNumber("-.00001e+5"));
assertEquals(0.00001,   toNumber(".001e-2"));
assertEquals(0.00001,   toNumber("+.001e-2"));
assertEquals(-0.00001,  toNumber("-.001e-2"));

assertEquals(12340000,   toNumber("1234e4"));
assertEquals(12340000,   toNumber("+1234e4"));
assertEquals(-12340000,  toNumber("-1234e4"));
assertEquals(12340000,   toNumber("1234e+4"));
assertEquals(12340000,   toNumber("+1234e+4"));
assertEquals(-12340000,  toNumber("-1234e+4"));
assertEquals(0.1234,     toNumber("1234e-4"));
assertEquals(0.1234,     toNumber("+1234e-4"));
assertEquals(-0.1234,    toNumber("-1234e-4"));

assertEquals(0,  toNumber("0x0"));
assertEquals(1,  toNumber("0x1"));
assertEquals(2,  toNumber("0x2"));
assertEquals(9,  toNumber("0x9"));
assertEquals(10, toNumber("0xa"));
assertEquals(11, toNumber("0xb"));
assertEquals(15, toNumber("0xf"));
assertEquals(10, toNumber("0xA"));
assertEquals(11, toNumber("0xB"));
assertEquals(15, toNumber("0xF"));

assertEquals(0,  toNumber("0X0"));
assertEquals(9,  toNumber("0X9"));
assertEquals(10, toNumber("0Xa"));
assertEquals(10, toNumber("0XA"));
assertEquals(15, toNumber("0Xf"));
assertEquals(15, toNumber("0XF"));

assertEquals(0,  toNumber("0x000"));
assertEquals(9,  toNumber("0x009"));
assertEquals(10, toNumber("0x00a"));
assertEquals(10, toNumber("0x00A"));
assertEquals(15, toNumber("0x00f"));
assertEquals(15, toNumber("0x00F"));

assertEquals(0, toNumber("00"));
assertEquals(1, toNumber("01"));
assertEquals(2, toNumber("02"));
assertEquals(10, toNumber("010"));
assertEquals(100, toNumber("0100"));
assertEquals(100, toNumber("000100"));

assertEquals(Infinity,  toNumber("1e999"), "1e999");
assertEquals(-Infinity, toNumber("-1e999"));
assertEquals(0,         toNumber("1e-999"));
assertEquals(0,         toNumber("-1e-999"));
assertEquals(Infinity,  1 / toNumber("1e-999"), "1e-999");
assertEquals(-Infinity, 1 / toNumber("-1e-999"));

assertTrue(isNaN(toNumber("junk")), "junk");
assertTrue(isNaN(toNumber("100 junk")), "100 junk");
assertTrue(isNaN(toNumber("0x100 junk")), "0x100 junk");
assertTrue(isNaN(toNumber("100.0 junk")), "100.0 junk");
assertTrue(isNaN(toNumber(".1e4 junk")), ".1e4 junk");
assertTrue(isNaN(toNumber("Infinity junk")), "Infinity junk");
