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

expected = ["A", undefined, "B", "bold", "/", "B", "and", undefined, "CODE", "coded", "/", "CODE", ""];
result = "A<B>bold</B>and<CODE>coded</CODE>".split(/<(\/)?([^<>]+)>/);
assertArrayEquals(expected, result);


assertArrayEquals(["a", "b"], "ab".split(/a*?/));

assertArrayEquals(["", "b"], "ab".split(/a*/));

assertArrayEquals(["a"], "ab".split(/a*?/, 1));

assertArrayEquals([""], "ab".split(/a*/, 1));

assertArrayEquals(["as","fas","fas","f"], "asdfasdfasdf".split("d"));

assertArrayEquals(["as","fas","fas","f"], "asdfasdfasdf".split("d", -1));

assertArrayEquals(["as", "fas"], "asdfasdfasdf".split("d", 2));

assertArrayEquals([], "asdfasdfasdf".split("d", 0));

assertArrayEquals(["as","fas","fas",""], "asdfasdfasd".split("d"));

assertArrayEquals([], "".split(""));

assertArrayEquals([""], "".split("a"));

assertArrayEquals(["a","b"], "axxb".split(/x*/));

assertArrayEquals(["a","b"], "axxb".split(/x+/));

assertArrayEquals(["a","","b"], "axxb".split(/x/));

// This was http://b/issue?id=1151354
assertArrayEquals(["div", "#id", ".class"], "div#id.class".split(/(?=[#.])/));


assertArrayEquals(["div", "#i", "d", ".class"], "div#id.class".split(/(?=[d#.])/));

assertArrayEquals(["a", "b", "c"], "abc".split(/(?=.)/));

assertArrayEquals(["Wenige", "sind", "auserwählt."],
                  "Wenige sind auserwählt.".split(" "));

assertArrayEquals([], "Wenige sind auserwählt.".split(" ", 0));

assertArrayEquals(["Wenige"], "Wenige sind auserwählt.".split(" ", 1));

assertArrayEquals(["Wenige", "sind"], "Wenige sind auserwählt.".split(" ", 2));

assertArrayEquals(["Wenige", "sind", "auserwählt."],
                  "Wenige sind auserwählt.".split(" ", 3));

assertArrayEquals(["Wenige sind auserw", "hlt."],
                  "Wenige sind auserwählt.".split("ä"));

assertArrayEquals(["Wenige sind ", "."],
                  "Wenige sind auserwählt.".split("auserwählt"));

/* "ab".split(/((?=.))/)
 *
 * KJS:   ,a,,b
 * SM:    a,,b,
 * IE:    a,b
 * Opera: a,,b
 * V8:    a,,b
 *
 * Opera seems to have this right.  The others make no sense.
 */
assertArrayEquals(["a", "", "b"], "ab".split(/((?=.))/));

/* "ab".split(/(?=)/)
 *
 * KJS:   a,b
 * SM:    ab
 * IE:    a,b
 * Opera: a,bb
 * V8:    a,b
 */
assertArrayEquals(["a", "b"], "ab".split(/(?=)/));


// For issue http://code.google.com/p/v8/issues/detail?id=924
// Splitting the empty string is a special case.
assertEquals([""], ''.split());
assertEquals([""], ''.split(/./));
assertEquals([], ''.split(/.?/));
assertEquals([], ''.split(/.??/));
assertEquals([], ''.split(/()()/));


// Issue http://code.google.com/p/v8/issues/detail?id=929
// (Splitting with empty separator and a limit.)

function numberObj(num) {
  return {valueOf: function() { return num; }};
}

assertEquals([], "abc".split("", 0));
assertEquals([], "abc".split("", numberObj(0)));
assertEquals(["a"], "abc".split("", 1));
assertEquals(["a"], "abc".split("", numberObj(1)));
assertEquals(["a", "b"], "abc".split("", 2));
assertEquals(["a", "b"], "abc".split("", numberObj(2)));
assertEquals(["a", "b", "c"], "abc".split("", 3));
assertEquals(["a", "b", "c"], "abc".split("", numberObj(3)));
assertEquals(["a", "b", "c"], "abc".split("", 4));
assertEquals(["a", "b", "c"], "abc".split("", numberObj(4)));

// Check if split works also for sliced strings.
let sliced_string = %ConstructSlicedString("abcdefghijklmnopqrstuvwxyz", 13);
assertEquals("nopqrstuvwxyz".split(""), sliced_string.split(""));
// Invoke twice for caching
assertEquals("nopqrstuvwxyz".split(""), sliced_string.split(""));

var all_ascii_codes = [];
for (var i = 0; i < 128; i++) all_ascii_codes[i] = i;
var all_ascii_string = String.fromCharCode.apply(String, all_ascii_codes);

var split_chars = all_ascii_string.split("");
assertEquals(128, split_chars.length);
for (var i = 0; i < 128; i++) {
  assertEquals(1, split_chars[i].length);
  assertEquals(i, split_chars[i].charCodeAt(0));
}

// Check that the separator is converted to string before returning due to
// limit == 0.
var counter = 0;
var separator = { toString: function() { counter++; return "b"; }};
assertEquals([], "abc".split(separator, 0));
assertEquals(1, counter);

// Check that the subject is converted to string before the separator.
counter = 0;
var subject = { toString: function() { assertEquals(0, counter);
                                       counter++;
                                       return "abc"; }};
separator = { toString: function() { assertEquals(1, counter);
                                     counter++;
                                     return "b"; }};

assertEquals(["a", "c"], String.prototype.split.call(subject, separator));
assertEquals(2, counter);

// Check ToUint32 conversion of limit.
assertArrayEquals(["a"], "a,b,c,d,e,f".split(/,/, -4294967295));
assertArrayEquals(["a", "b"], "a,b,c,d,e,f".split(/,/, -4294967294.001));
assertArrayEquals(["a", "b"], "a,b,c,d,e,f".split(/,/, -4294967294.5));
assertArrayEquals(["a", "b"], "a,b,c,d,e,f".split(/,/, -4294967294.999));
assertArrayEquals(["a", "b"], "a,b,c,d,e,f".split(/,/, -4294967294));
assertArrayEquals(["a", "b", "c"], "a,b,c,d,e,f".split(/,/, -4294967293));
assertArrayEquals(["a", "b", "c", "d"], "a,b,c,d,e,f".split(/,/, -4294967292));
assertArrayEquals(["a", "b", "c", "d", "e", "f"], "a,b,c,d,e,f".split(/,/, -1));
assertArrayEquals(["a", "b", "c"], "abc".split("", 0xffffffff));
assertArrayEquals(["\u0427", "\u0427"], "\u0427\u0427".split("", 0xffffffff));
