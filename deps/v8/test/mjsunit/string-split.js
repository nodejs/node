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
assertArrayEquals(expected, result, 1);

expected = ["a", "b"];
result = "ab".split(/a*?/);
assertArrayEquals(expected, result, 2);

expected = ["", "b"];
result = "ab".split(/a*/);
assertArrayEquals(expected, result, 3);

expected = ["a"];
result = "ab".split(/a*?/, 1);
assertArrayEquals(expected, result, 4);

expected = [""];
result = "ab".split(/a*/, 1);
assertArrayEquals(expected, result, 5);

expected = ["as","fas","fas","f"];
result = "asdfasdfasdf".split("d");
assertArrayEquals(expected, result, 6);

expected = ["as","fas","fas","f"];
result = "asdfasdfasdf".split("d", -1);
assertArrayEquals(expected, result, 7);

expected = ["as", "fas"];
result = "asdfasdfasdf".split("d", 2);
assertArrayEquals(expected, result, 8);

expected = [];
result = "asdfasdfasdf".split("d", 0);
assertArrayEquals(expected, result, 9);

expected = ["as","fas","fas",""];
result = "asdfasdfasd".split("d");
assertArrayEquals(expected, result, 10);

expected = [];
result = "".split("");
assertArrayEquals(expected, result, 11);

expected = [""]
result = "".split("a");
assertArrayEquals(expected, result, 12);

expected = ["a","b"]
result = "axxb".split(/x*/);
assertArrayEquals(expected, result, 13);

expected = ["a","b"]
result = "axxb".split(/x+/);
assertArrayEquals(expected, result, 14);

expected = ["a","","b"]
result = "axxb".split(/x/);
assertArrayEquals(expected, result, 15);

// This was http://b/issue?id=1151354
expected = ["div", "#id", ".class"]
result = "div#id.class".split(/(?=[#.])/);
assertArrayEquals(expected, result, 16);

expected = ["div", "#i", "d", ".class"]
result = "div#id.class".split(/(?=[d#.])/);
assertArrayEquals(expected, result, 17);

expected = ["a", "b", "c"]
result = "abc".split(/(?=.)/);
assertArrayEquals(expected, result, 18);

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
expected = ["a", "", "b"]
result = "ab".split(/((?=.))/);
assertArrayEquals(expected, result, 19);

/* "ab".split(/(?=)/)
 *
 * KJS:   a,b
 * SM:    ab
 * IE:    a,b
 * Opera: a,b
 * V8:    a,b
 */
expected = ["a", "b"]
result = "ab".split(/(?=)/);
assertArrayEquals(expected, result, 20);

