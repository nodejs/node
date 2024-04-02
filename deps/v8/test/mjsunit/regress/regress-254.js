// Copyright 2009 the V8 project authors. All rights reserved.
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

// See: http://code.google.com/p/v8/issues/detail?id=254

// RegExp with global flag: exec and test updates lastIndex.
var re = /x/g;

assertEquals(0, re.lastIndex, "Global, initial lastIndex");

assertTrue(re.test("x"), "Global, test 1");
assertEquals(1, re.lastIndex, "Global, lastIndex after test 1");
assertFalse(re.test("x"), "Global, test 2");
assertEquals(0, re.lastIndex, "Global, lastIndex after test 2");

assertEquals(["x"], re.exec("x"), "Global, exec 1");
assertEquals(1, re.lastIndex, "Global, lastIndex after exec 1");
assertEquals(null, re.exec("x"), "Global, exec 2");
assertEquals(0, re.lastIndex, "Global, lastIndex after exec 2");

// RegExp without global flag: exec and test leavs lastIndex at zero.
var re2 = /x/;

assertEquals(0, re2.lastIndex, "Non-global, initial lastIndex");

assertTrue(re2.test("x"), "Non-global, test 1");
assertEquals(0, re2.lastIndex, "Non-global, lastIndex after test 1");
assertTrue(re2.test("x"), "Non-global, test 2");
assertEquals(0, re2.lastIndex, "Non-global, lastIndex after test 2");

assertEquals(["x"], re2.exec("x"), "Non-global, exec 1");
assertEquals(0, re2.lastIndex, "Non-global, lastIndex after exec 1");
assertEquals(["x"], re2.exec("x"), "Non-global, exec 2");
assertEquals(0, re2.lastIndex, "Non-global, lastIndex after exec 2");
