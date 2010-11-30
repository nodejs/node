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
