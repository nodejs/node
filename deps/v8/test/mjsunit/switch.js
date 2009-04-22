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

function f0() {
  switch (0) {
    // switch deliberately left empty
  }
}

f0();  // no errors

function f1(x) {
  switch (x) {
    default:      return "f1";
  }
  return "foo";
}

assertEquals("f1", f1(0), "default-switch.0");
assertEquals("f1", f1(1), "default-switch.1");

function f2(x) {
  var r;
  switch (x) {
    case 0:
      r = "zero";
      break;
    case 1:
      r = "one";
      break;
    case 2:
      r = "two";
      break;
    case 3:
      r = "three";
      break;
    default:
      r = "default";
  }
  return r;
}

assertEquals("zero", f2(0), "0-1-switch.0");
assertEquals("one", f2(1), "0-1-switch.1");
assertEquals("default", f2(7), "0-1-switch.2");
assertEquals("default", f2(-1), "0-1-switch.-1");
assertEquals("default", f2(NaN), "0-1-switch.NaN");
assertEquals("default", f2(Math.pow(2,34)), "0-1-switch.largeNum");
assertEquals("default", f2("0"), "0-1-switch.string");
assertEquals("default", f2(false), "0-1-switch.bool");
assertEquals("default", f2(null), "0-1-switch.null");
assertEquals("default", f2(undefined), "0-1-switch.undef");
assertEquals("default", f2(new Number(2)), "0-1-switch.undef");
assertEquals("default", f2({valueOf: function(){return 2; }}), "0-1-switch.obj");


function f3(x, c) {
  var r = 0;
  switch (x) {
    default:
      r = "default";
      break;
    case  c:
      r = "value is c = " + c;
      break;
    case  2:
      r = "two";
      break;
    case -5:
      r = "minus 5";
      break;
    case  9:
      r = "nine";
      break;
  }
  return r;
}

assertEquals("two", f3(2,0), "value-switch.2-0");
assertEquals("minus 5", f3(-5,0), "value-switch.-5-0");
assertEquals("nine", f3(9,0), "value-switch.9-0");
assertEquals("value is c = 0", f3(0,0), "value-switch.0-0");
assertEquals("value is c = 2", f3(2,2), "value-switch.2-2");
assertEquals("default", f3(7,0), "value-switch.7-0");


function f4(x) {
  switch(x) {
    case 0:
      x++;
    default:
      x++;
    case 2:
      x++;
  }
  return x;
}


assertEquals(3, f4(0), "fallthrough-switch.0");
assertEquals(3, f4(1), "fallthrough-switch.1");
assertEquals(3, f4(2), "fallthrough-switch.2");
assertEquals(5, f4(3), "fallthrough-switch.3");


function f5(x) {
  switch(x) {
     case -2: return true;
     case -1: return false;
     case 0: return true;
     case 2: return false;
     default: return 42;
  }
}

assertTrue(f5(-2), "negcase.-2");
assertFalse(f5(-1), "negcase.-1");
assertTrue(f5(0), "negcase.-0");
assertEquals(42, f5(1), "negcase.1");
assertFalse(f5(2), "negcase.2");

function f6(N) {
  // long enough case that code buffer grows during code-generation
  var res = 0;
  for(var i = 0; i < N; i++) {
    switch(i & 0x3f) {
    case 0: res += 0; break;
    case 1: res += 1; break;
    case 2: res += 2; break;
    case 3: res += 3; break;
    case 4: res += 4; break;
    case 5: res += 5; break;
    case 6: res += 6; break;
    case 7: res += 7; break;
    case 8: res += 8; break;
    case 9: res += 9; break;
    case 10: res += 10; break;
    case 11: res += 11; break;
    case 12: res += 12; break;
    case 13: res += 13; break;
    case 14: res += 14; break;
    case 15: res += 15; break;
    case 16: res += 16; break;
    case 17: res += 17; break;
    case 18: res += 18; break;
    case 19: res += 19; break;
    case 20: res += 20; break;
    case 21: res += 21; break;
    case 22: res += 22; break;
    case 23: res += 23; break;
    case 24: res += 24; break;
    case 25: res += 25; break;
    case 26: res += 26; break;
    case 27: res += 27; break;
    case 28: res += 28; break;
    case 29: res += 29; break;
    case 30: res += 30; break;
    case 31: res += 31; break;
    case 32: res += 32; break;
    case 33: res += 33; break;
    case 34: res += 34; break;
    case 35: res += 35; break;
    case 36: res += 36; break;
    case 37: res += 37; break;
    case 38: res += 38; break;
    case 39: res += 39; break;
    case 40: res += 40; break;
    case 41: res += 41; break;
    case 42: res += 42; break;
    case 43: res += 43; break;
    case 44: res += 44; break;
    case 45: res += 45; break;
    case 46: res += 46; break;
    case 47: res += 47; break;
    case 48: res += 48; break;
    case 49: res += 49; break;
    case 50: res += 50; break;
    case 51: res += 51; break;
    case 52: res += 52; break;
    case 53: res += 53; break;
    case 54: res += 54; break;
    case 55: res += 55; break;
    case 56: res += 56; break;
    case 57: res += 57; break;
    case 58: res += 58; break;
    case 59: res += 59; break;
    case 60: res += 60; break;
    case 61: res += 61; break;
    case 62: res += 62; break;
    case 63: res += 63; break;
    case 64: break;
    default: break;
    }
  }
  return res;
}

assertEquals(190, f6(20), "largeSwitch.20");
assertEquals(2016, f6(64), "largeSwitch.64");
assertEquals(4032, f6(128), "largeSwitch.128");
assertEquals(4222, f6(148), "largeSwitch.148");


function f7(value) {
  switch (value) {
  case 0: return "0";
  case -0: return "-0";
  case 1: case 2: case 3: case 4:  // Dummy fillers.
  }
  switch (value) {
  case 0x3fffffff: return "MaxSmi";
  case 0x3ffffffe:
  case 0x3ffffffd:
  case 0x3ffffffc:
  case 0x3ffffffb:
  case 0x3ffffffa:  // Dummy fillers
  }
  switch (value) {
  case -0x40000000: return "MinSmi";
  case -0x3fffffff:
  case -0x3ffffffe:
  case -0x3ffffffd:
  case -0x3ffffffc:
  case -0x3ffffffb:  // Dummy fillers
  }
  switch (value) {
  case 10: return "A";
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:  // Dummy fillers
  }
  return "default";
}


assertEquals("default", f7(0.1), "0-1-switch.double-0.1");
assertEquals("0", f7(-0), "0-1-switch.double-neg0");
assertEquals("MaxSmi", f7((1<<30)-1), "0-1-switch.maxsmi");
assertEquals("MinSmi", f7(-(1<<30)), "0-1-switch.minsmi");
assertEquals("default", f7(1<<30), "0-1-switch.maxsmi++");
assertEquals("default", f7(-(1<<30)-1), "0-1-switch.minsmi--");
assertEquals("A", f7((170/16)-(170%16/16)), "0-1-switch.heapnum");


function makeVeryLong(length) {
  var res = "function() {\n" +
            "  var res = 0;\n" +
            "  for (var i = 0; i <= " + length + "; i++) {\n" +
            "    switch(i) {\n";
  for (var i = 0; i < length; i++) {
    res += "    case " + i + ": res += 2; break;\n";
  }
  res += "    default: res += 1;\n" +
         "    }\n" +
         "  }\n" +
         "  return res;\n" +
         "}";
  return eval(res);
}
var verylong_size = 1000;
var verylong = makeVeryLong(verylong_size);

assertEquals(verylong_size * 2 + 1, verylong());
