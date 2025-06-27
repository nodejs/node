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

// Flags: --allow-natives-syntax

var s = "test test test";

assertEquals(0, s.indexOf("t"));
assertEquals(3, s.indexOf("t", 1));
assertEquals(5, s.indexOf("t", 4));
assertEquals(5, s.indexOf("t", 4.1));
assertEquals(0, s.indexOf("t", 0));
assertEquals(0, s.indexOf("t", -1));
assertEquals(0, s.indexOf("t", -1));
assertEquals(0, s.indexOf("t", -1.1));
assertEquals(0, s.indexOf("t", -1073741825));
assertEquals(1, s.indexOf("e"));
assertEquals(2, s.indexOf("s"));

assertEquals(5, s.indexOf("test", 4));
assertEquals(5, s.indexOf("test", 5));
assertEquals(10, s.indexOf("test", 6));
assertEquals(10, s.indexOf("test", 6.0));
assertEquals(0, s.indexOf("test", 0));
assertEquals(0, s.indexOf("test", 0.0));
assertEquals(0, s.indexOf("test", -1));
assertEquals(-1, s.indexOf("not found", -1));
assertEquals(0, s.indexOf("test", -1.0));
assertEquals(0, s.indexOf("test", -1073741825));
assertEquals(0, s.indexOf("test"));
assertEquals(-1, s.indexOf("notpresent"));
assertEquals(-1, s.indexOf());

for (var i = 0; i < s.length+10; i++) {
  var expected = i < s.length ? i : s.length;
  assertEquals(expected, s.indexOf("", i));
}

var reString = "asdf[a-z]+(asdf)?";

assertEquals(4, reString.indexOf("[a-z]+"));
assertEquals(10, reString.indexOf("(asdf)?"));

assertEquals(1, String.prototype.indexOf.length);

// Random greek letters
var twoByteString = "\u039a\u0391\u03a3\u03a3\u0395";

// Test single char pattern
assertEquals(0, twoByteString.indexOf("\u039a"), "Lamda");
assertEquals(1, twoByteString.indexOf("\u0391"), "Alpha");
assertEquals(2, twoByteString.indexOf("\u03a3"), "First Sigma");
assertEquals(3, twoByteString.indexOf("\u03a3",3), "Second Sigma");
assertEquals(4, twoByteString.indexOf("\u0395"), "Epsilon");
assertEquals(-1, twoByteString.indexOf("\u0392"), "Not beta");

// Test multi-char pattern
assertEquals(0, twoByteString.indexOf("\u039a\u0391"), "lambda Alpha");
assertEquals(1, twoByteString.indexOf("\u0391\u03a3"), "Alpha Sigma");
assertEquals(2, twoByteString.indexOf("\u03a3\u03a3"), "Sigma Sigma");
assertEquals(3, twoByteString.indexOf("\u03a3\u0395"), "Sigma Epsilon");

assertEquals(-1, twoByteString.indexOf("\u0391\u03a3\u0395"),
    "Not Alpha Sigma Epsilon");

//single char pattern
assertEquals(4, twoByteString.indexOf("\u0395"));

// test string with alignment traps
var alignmentString = "\u1122\u2211\u2222\uFF00\u00FF\u00FF";
assertEquals(2, alignmentString.indexOf("\u2222"));
assertEquals(4, alignmentString.indexOf("\u00FF\u00FF"));

var longAlignmentString = "\uFF00" + "\u00FF".repeat(10);
assertEquals(1,
    longAlignmentString.indexOf("\u00FF".repeat(10)));

// test string with first character match at the end
var boundsString = "112233";
assertEquals(-1, boundsString.indexOf("334455"));
assertEquals(-1, boundsString.indexOf("334455".repeat(10)));

// Test complex string indexOf algorithms. Only trigger for long strings.

// Long string that isn't a simple repeat of a shorter string.
var long = "A";
for(var i = 66; i < 76; i++) {  // from 'B' to 'K'
  long =  long + String.fromCharCode(i) + long;
}

// pattern of 15 chars, repeated every 16 chars in long
var pattern = "ABACABADABACABA";
for(var i = 0; i < long.length - pattern.length; i+= 7) {
  var index = long.indexOf(pattern, i);
  assertEquals((i + 15) & ~0xf, index, "Long ABACABA...-string at index " + i);
}
assertEquals(510, long.indexOf("AJABACA"), "Long AJABACA, First J");
assertEquals(1534, long.indexOf("AJABACA", 511), "Long AJABACA, Second J");

pattern = "JABACABADABACABA";
assertEquals(511, long.indexOf(pattern), "Long JABACABA..., First J");
assertEquals(1535, long.indexOf(pattern, 512), "Long JABACABA..., Second J");


// Search for a non-ASCII string in a pure ASCII string.
var asciiString = "arglebargleglopglyfarglebargleglopglyfarglebargleglopglyf";
assertEquals(-1, asciiString.indexOf("\x2061"));


// Search in string containing many non-ASCII chars.
var allCodePoints = [];
for (var i = 0; i < 65536; i++) allCodePoints[i] = i;
var allCharsString = String.fromCharCode.apply(String, allCodePoints);
// Search for string long enough to trigger complex search with ASCII pattern
// and UC16 subject.
assertEquals(-1, allCharsString.indexOf("notfound"));

// Find substrings.
var lengths = [1, 4, 15];  // Single char, simple and complex.
var indices = [0x5, 0x65, 0x85, 0x105, 0x205, 0x285, 0x2005, 0x2085, 0xfff0];
for (var lengthIndex = 0; lengthIndex < lengths.length; lengthIndex++) {
  var length = lengths[lengthIndex];
  for (var i = 0; i < indices.length; i++) {
    var index = indices[i];
    var pattern = allCharsString.substring(index, index + length);
    assertEquals(index, allCharsString.indexOf(pattern));
  }
}

(function nonStringReceivers() {
  let indexOf = String.prototype.indexOf;
  assertThrows(() => indexOf.call(null), TypeError);
  assertThrows(() => indexOf.call(undefined), TypeError);

  assertEquals(-1, indexOf.call(1));
  assertEquals(0, indexOf.call(1, "1"));

  assertEquals(-1, indexOf.call(1.2));
  assertEquals(0, indexOf.call(1.2, "1"));
  assertEquals(1, indexOf.call(1.2, "."));
  assertEquals(2, indexOf.call(1.2, "2"));
  assertEquals(-1, indexOf.call(1.2, "1", 2));

  assertEquals(-1, indexOf.call({}));
  assertEquals(0, indexOf.call({}, "[object Object]"));
  assertEquals(-1, indexOf.call({}, "[object", 1));

  assertEquals(-1, indexOf.call([]));
  assertEquals(0, indexOf.call([1,2], "1,2"));

  assertEquals(-1, indexOf.call(this));
})();

(function nonStringSearchString() {

  assertEquals(-1, "".indexOf(1));
  assertEquals(2, "_0123".indexOf(1));

  assertEquals(-1, "".indexOf(1.2));
  assertEquals(1, "01.2".indexOf(1.2));
  assertEquals(1, "01.2".indexOf(1.2, 1));
  assertEquals(-1, "01.2".indexOf(1.2, 2));

  assertEquals(-1, "".indexOf(null));
  assertEquals(0, "null".indexOf(null));

  assertEquals(-1, "".indexOf(undefined));
  assertEquals(1, "_undefined_".indexOf(undefined));

  assertEquals(0, "".indexOf([]));
  assertEquals(0, "123".indexOf([]));
  assertEquals(2, "1,2,3".indexOf([2,3]));

  assertEquals(-1, "".indexOf({}));
  assertEquals(-1, "".indexOf(this));
})();

(function nonStringPosition() {
  assertEquals(0, "aba".indexOf("a", false));
  assertEquals(2, "aba".indexOf("a", true));
  assertEquals(2, "aba".indexOf("a", "1"));
  assertEquals(2, "aba".indexOf("a", "1.00000"));
  assertEquals(2, "aba".indexOf("a", "2.00000"));
  assertEquals(-1, "aba".indexOf("a", "3.00000"));
})();

(function optimize() {
  function f() {
    return 'abc'.indexOf('a');
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(0, f());
  assertEquals(0, f());
  assertEquals(0, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f());

  function f2() {
    return 'abc'.indexOf('a', 1);
  }
  %PrepareFunctionForOptimization(f2);
  assertEquals(-1, f2());
  assertEquals(-1, f2());
  assertEquals(-1, f2());
  %OptimizeFunctionOnNextCall(f2);
  assertEquals(-1, f2());

  function f3() {
    return 'abc'.indexOf('a');
  }
  %PrepareFunctionForOptimization(f3);
  assertEquals(0, f3());
  assertEquals(0, f3());
  assertEquals(0, f3());
  %OptimizeFunctionOnNextCall(f3);
  assertEquals(0, f3());

  function f4() {
    return 'abcbc'.indexOf('bc', 2);
  }
  %PrepareFunctionForOptimization(f4);
  assertEquals(3, f4());
  assertEquals(3, f4());
  assertEquals(3, f4());
  %OptimizeFunctionOnNextCall(f4);
  assertEquals(3, f4());

  function f5() {
    return 'abcbc'.indexOf('b', -1);
  }
  %PrepareFunctionForOptimization(f5);
  assertEquals(1, f5());
  assertEquals(1, f5());
  assertEquals(1, f5());
  %OptimizeFunctionOnNextCall(f5);
  assertEquals(1, f5());

  function f6() {
    return 'abcbc'.indexOf('b', -10737418);
  }
  %PrepareFunctionForOptimization(f6);
  assertEquals(1, f6());
  assertEquals(1, f6());
  assertEquals(1, f6());
  %OptimizeFunctionOnNextCall(f6);
  assertEquals(1, f6());
})();

(function optimizeOSR() {
  function f() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abc'.indexOf('a');
    }
    return result;
  }
  assertEquals(0, f());

  function f2() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abc'.indexOf('a', 1);
    }
    return result;
  }
  assertEquals(-1, f2());

  function f3() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abc'.indexOf('a');
    }
    return result;
  }
  assertEquals(0, f3());

  function f4() {
    var result;
    for (var i = 0; i < 100000; i++) {
      result = 'abcbc'.indexOf('bc', 2);
    }
    return result;
  }
  assertEquals(3, f4());
})();
