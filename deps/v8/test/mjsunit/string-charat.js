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

var s = "test";

function getTwoByteString() { return "\u1234t"; }
function getCons() { return "testtesttesttest" + getTwoByteString() }

var slowIndex1 = { valueOf: function() { return 1; } };
var slowIndex2 = { toString: function() { return "2"; } };
var slowIndexOutOfRange = { valueOf: function() { return -1; } };

function basicTest(s, len) {
  assertEquals("t", s().charAt());
  assertEquals("t", s().charAt("string"));
  assertEquals("t", s().charAt(null));
  assertEquals("t", s().charAt(void 0));
  assertEquals("t", s().charAt(false));
  assertEquals("e", s().charAt(true));
  assertEquals("", s().charAt(-1));
  assertEquals("", s().charAt(len));
  assertEquals("", s().charAt(slowIndexOutOfRange));
  assertEquals("", s().charAt(1/0));
  assertEquals("", s().charAt(-1/0));
  assertEquals("t", s().charAt(0));
  assertEquals("t", s().charAt(-0.0));
  assertEquals("t", s().charAt(-0.1));
  assertEquals("t", s().charAt(0.4));
  assertEquals("e", s().charAt(slowIndex1));
  assertEquals("s", s().charAt(slowIndex2));
  assertEquals("t", s().charAt(3));
  assertEquals("t", s().charAt(3.4));
  assertEquals("t", s().charAt(NaN));

  assertEquals(116, s().charCodeAt());
  assertEquals(116, s().charCodeAt("string"));
  assertEquals(116, s().charCodeAt(null));
  assertEquals(116, s().charCodeAt(void 0));
  assertEquals(116, s().charCodeAt(false));
  assertEquals(101, s().charCodeAt(true));
  assertEquals(116, s().charCodeAt(0));
  assertEquals(116, s().charCodeAt(-0.0));
  assertEquals(116, s().charCodeAt(-0.1));
  assertEquals(116, s().charCodeAt(0.4));
  assertEquals(101, s().charCodeAt(slowIndex1));
  assertEquals(115, s().charCodeAt(slowIndex2));
  assertEquals(116, s().charCodeAt(3));
  assertEquals(116, s().charCodeAt(3.4));
  assertEquals(116, s().charCodeAt(NaN));
  assertTrue(isNaN(s().charCodeAt(-1)));
  assertTrue(isNaN(s().charCodeAt(len)));
  assertTrue(isNaN(s().charCodeAt(slowIndexOutOfRange)));
  assertTrue(isNaN(s().charCodeAt(1/0)));
  assertTrue(isNaN(s().charCodeAt(-1/0)));
}
basicTest(function() { return s; }, s.length);
basicTest(getCons, getCons().length);

// Make sure enough of the one-char string cache is filled.
var alpha = ['@'];
for (var i = 1; i < 128; i++) {
  var c = String.fromCharCode(i);
  alpha[i] = c.charAt(0);
}
var alphaStr = alpha.join("");

// Now test chars.
for (var i = 1; i < 128; i++) {
  assertEquals(alpha[i], alphaStr.charAt(i));
  assertEquals(String.fromCharCode(i), alphaStr.charAt(i));
}

// Test stealing String.prototype.{charAt,charCodeAt}.
var o = {
  charAt: String.prototype.charAt,
  charCodeAt: String.prototype.charCodeAt,
  toString: function() { return "012"; },
  valueOf: function() { return "should not be called"; }
};

function stealTest() {
  assertEquals("0", o.charAt(0));
  assertEquals("1", o.charAt(1));
  assertEquals("1", o.charAt(1.4));
  assertEquals("1", o.charAt(slowIndex1));
  assertEquals("2", o.charAt(2));
  assertEquals("2", o.charAt(slowIndex2));
  assertEquals(48, o.charCodeAt(0));
  assertEquals(49, o.charCodeAt(1));
  assertEquals(49, o.charCodeAt(1.4));
  assertEquals(49, o.charCodeAt(slowIndex1));
  assertEquals(50, o.charCodeAt(2));
  assertEquals(50, o.charCodeAt(slowIndex2));
  assertEquals("", o.charAt(-1));
  assertEquals("", o.charAt(-1.4));
  assertEquals("", o.charAt(10));
  assertEquals("", o.charAt(slowIndexOutOfRange));
  assertTrue(isNaN(o.charCodeAt(-1)));
  assertTrue(isNaN(o.charCodeAt(-1.4)));
  assertTrue(isNaN(o.charCodeAt(10)));
  assertTrue(isNaN(o.charCodeAt(slowIndexOutOfRange)));
}
stealTest();

// Test custom string IC-s.
for (var i = 0; i < 20; i++) {
  basicTest(function() { return s; }, s.length);
  basicTest(getCons, getCons().length);
  stealTest();
}

var badToString = function() { return []; };

function testBadToString_charAt() {
  var goodToString = o.toString;
  var hasCaught = false;
  var numCalls = 0;
  var result;
  try {
    for (var i = 0; i < 20; i++) {
      if (i == 10) o.toString = o.valueOf = badToString;
      result = o.charAt(1);
      numCalls++;
    }
  } catch (e) {
    hasCaught = true;
  } finally {
    o.toString = goodToString;
  }
  assertTrue(hasCaught);
  assertEquals("1", result);
  assertEquals(10, numCalls);
}
testBadToString_charAt();

function testBadToString_charCodeAt() {
  var goodToString = o.toString;
  var hasCaught = false;
  var numCalls = 0;
  var result;
  try {
    for (var i = 0; i < 20; i++) {
      if (i == 10) o.toString = o.valueOf = badToString;
      result = o.charCodeAt(1);
      numCalls++;
    }
  } catch (e) {
    hasCaught = true;
  } finally {
    o.toString = goodToString;
  }
  assertTrue(hasCaught);
  assertEquals(49, result);
  assertEquals(10, numCalls);
}
testBadToString_charCodeAt();

var badIndex = {
  toString: badToString,
  valueOf: badToString
};

function testBadIndex_charAt() {
  var index = 1;
  var hasCaught = false;
  var numCalls = 0;
  var result;
  try {
    for (var i = 0; i < 20; i++) {
      if (i == 10) index = badIndex;
      result = o.charAt(index);
      numCalls++;
    }
  } catch (e) {
    hasCaught = true;
  }
  assertTrue(hasCaught);
  assertEquals("1", result);
  assertEquals(10, numCalls);
}
testBadIndex_charAt();

function testBadIndex_charCodeAt() {
  var index = 1;
  var hasCaught = false;
  var numCalls = 0;
  var result;
  try {
    for (var i = 0; i < 20; i++) {
      if (i == 10) index = badIndex;
      result = o.charCodeAt(index);
      numCalls++;
    }
  } catch (e) {
    hasCaught = true;
  }
  assertTrue(hasCaught);
  assertEquals(49, result);
  assertEquals(10, numCalls);
}
testBadIndex_charCodeAt();

function testPrototypeChange_charAt() {
  var result, oldResult;
  for (var i = 0; i < 20; i++) {
    if (i == 10) {
      oldResult = result;
      String.prototype.charAt = function() { return "%"; };
    }
    result = s.charAt(1);
  }
  assertEquals("%", result);
  assertEquals("e", oldResult);
  delete String.prototype.charAt;  // Restore the default.
}
testPrototypeChange_charAt();

function testPrototypeChange_charCodeAt() {
  var result, oldResult;
  for (var i = 0; i < 20; i++) {
    if (i == 10) {
      oldResult = result;
      String.prototype.charCodeAt = function() { return 42; };
    }
    result = s.charCodeAt(1);
  }
  assertEquals(42, result);
  assertEquals(101, oldResult);
  delete String.prototype.charCodeAt;  // Restore the default.
}
testPrototypeChange_charCodeAt();
