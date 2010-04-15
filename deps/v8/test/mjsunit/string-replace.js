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

/**
 * @fileoverview Test String.prototype.replace
 */

function replaceTest(result, subject, pattern, replacement) {
  var name = 
    "\"" + subject + "\".replace(" + pattern + ", " + replacement + ")";
  assertEquals(result, subject.replace(pattern, replacement), name);
}


var short = "xaxbxcx";

replaceTest("axbxcx", short, "x", "");
replaceTest("axbxcx", short, /x/, "");
replaceTest("abc", short, /x/g, "");

replaceTest("xaxxcx", short, "b", "");
replaceTest("xaxxcx", short, /b/, "");
replaceTest("xaxxcx", short, /b/g, "");


replaceTest("[]axbxcx", short, "x", "[]");
replaceTest("[]axbxcx", short, /x/, "[]");
replaceTest("[]a[]b[]c[]", short, /x/g, "[]");

replaceTest("xax[]xcx", short, "b", "[]");
replaceTest("xax[]xcx", short, /b/, "[]");
replaceTest("xax[]xcx", short, /b/g, "[]");


replaceTest("[$]axbxcx", short, "x", "[$$]");
replaceTest("[$]axbxcx", short, /x/, "[$$]");
replaceTest("[$]a[$]b[$]c[$]", short, /x/g, "[$$]");

replaceTest("xax[$]xcx", short, "b", "[$$]");
replaceTest("xax[$]xcx", short, /b/, "[$$]");
replaceTest("xax[$]xcx", short, /b/g, "[$$]");


replaceTest("[]axbxcx", short, "x", "[$`]");
replaceTest("[]axbxcx", short, /x/, "[$`]");
replaceTest("[]a[xa]b[xaxb]c[xaxbxc]", short, /x/g, "[$`]");

replaceTest("xax[xax]xcx", short, "b", "[$`]");
replaceTest("xax[xax]xcx", short, /b/, "[$`]");
replaceTest("xax[xax]xcx", short, /b/g, "[$`]");


replaceTest("[x]axbxcx", short, "x", "[$&]");
replaceTest("[x]axbxcx", short, /x/, "[$&]");
replaceTest("[x]a[x]b[x]c[x]", short, /x/g, "[$&]");

replaceTest("xax[b]xcx", short, "b", "[$&]");
replaceTest("xax[b]xcx", short, /b/, "[$&]");
replaceTest("xax[b]xcx", short, /b/g, "[$&]");


replaceTest("[axbxcx]axbxcx", short, "x", "[$']");
replaceTest("[axbxcx]axbxcx", short, /x/, "[$']");
replaceTest("[axbxcx]a[bxcx]b[cx]c[]", short, /x/g, "[$']");

replaceTest("xax[xcx]xcx", short, "b", "[$']");
replaceTest("xax[xcx]xcx", short, /b/, "[$']");
replaceTest("xax[xcx]xcx", short, /b/g, "[$']");


replaceTest("[$1]axbxcx", short, "x", "[$1]");
replaceTest("[$1]axbxcx", short, /x/, "[$1]");
replaceTest("[]axbxcx", short, /x()/, "[$1]");
replaceTest("[$1]a[$1]b[$1]c[$1]", short, /x/g, "[$1]");
replaceTest("[]a[]b[]c[]", short, /x()/g, "[$1]");

replaceTest("xax[$1]xcx", short, "b", "[$1]");
replaceTest("xax[$1]xcx", short, /b/, "[$1]");
replaceTest("xax[]xcx", short, /b()/, "[$1]");
replaceTest("xax[$1]xcx", short, /b/g, "[$1]");
replaceTest("xax[]xcx", short, /b()/g, "[$1]");

// Bug 317 look-alikes. If "$e" has no meaning, the "$" must be retained.
replaceTest("xax$excx", short, "b", "$e");
replaceTest("xax$excx", short, /b/, "$e");
replaceTest("xax$excx", short, /b/g, "$e");

replaceTest("xaxe$xcx", short, "b", "e$");
replaceTest("xaxe$xcx", short, /b/, "e$");
replaceTest("xaxe$xcx", short, /b/g, "e$");


replaceTest("[$$$1$$a1abb1bb0$002$3$03][$$$1$$b1bcc1cc0$002$3$03]c", 
            "abc", /(.)(?=(.))/g, "[$$$$$$1$$$$$11$01$2$21$02$020$002$3$03]"); 

// Replace with functions.


var ctr = 0;
replaceTest("0axbxcx", short, "x", function r(m, i, s) {
  assertEquals(3, arguments.length, "replace('x',func) func-args");
  assertEquals("x", m, "replace('x',func(m,..))");
  assertEquals(0, i, "replace('x',func(..,i,..))");
  assertEquals(short, s, "replace('x',func(..,s))");
  return String(ctr++);
});
assertEquals(1, ctr, "replace('x',func) num-match");

ctr = 0;
replaceTest("0axbxcx", short, /x/, function r(m, i, s) {
  assertEquals(3, arguments.length, "replace(/x/,func) func-args");
  assertEquals("x", m, "replace(/x/,func(m,..))");
  assertEquals(0, i, "replace(/x/,func(..,i,..))");
  assertEquals(short, s, "replace(/x/,func(..,s))");
  return String(ctr++);
});
assertEquals(1, ctr, "replace(/x/,func) num-match");

ctr = 0;
replaceTest("0a1b2c3", short, /x/g, function r(m, i, s) {
  assertEquals(3, arguments.length, "replace(/x/g,func) func-args");
  assertEquals("x", m, "replace(/x/g,func(m,..))");
  assertEquals(ctr * 2, i, "replace(/x/g,func(..,i,.))");
  assertEquals(short, s, "replace(/x/g,func(..,s))");
  return String(ctr++);
});
assertEquals(4, ctr, "replace(/x/g,func) num-match");

ctr = 0;
replaceTest("0a1b2cx", short, /(x)(?=(.))/g, function r(m, c1, c2, i, s) {
  assertEquals(5, arguments.length, "replace(/(x)(?=(.))/g,func) func-args");
  assertEquals("x", m, "replace(/(x)(?=(.))/g,func(m,..))");
  assertEquals("x", c1, "replace(/(x)(?=(.))/g,func(..,c1,..))");
  assertEquals(["a","b","c"][ctr], c2, "replace(/(x)(?=(.))/g,func(..,c2,..))");
  assertEquals(ctr * 2, i, "replace(/(x)(?=(.))/g,func(..,i,..))");
  assertEquals(short, s, "replace(/(x)(?=(.))/g,func(..,s))");
  return String(ctr++);
});
assertEquals(3, ctr, "replace(/x/g,func) num-match");


// Test special cases of replacement parts longer than 1<<11.
var longstring = "xyzzy";
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
longstring = longstring + longstring;
// longstring.length == 5 << 11

replaceTest(longstring + longstring,
            "<" + longstring + ">", /<(.*)>/g, "$1$1");

replaceTest("string 42", "string x", /x/g, function() { return 42; });
replaceTest("string 42", "string x", /x/, function() { return 42; });
replaceTest("string 42", "string x", /[xy]/g, function() { return 42; });
replaceTest("string 42", "string x", /[xy]/, function() { return 42; });
replaceTest("string true", "string x", /x/g, function() { return true; });
replaceTest("string null", "string x", /x/g, function() { return null; });
replaceTest("string undefined", "string x", /x/g, function() { return undefined; });

replaceTest("aundefinedbundefinedcundefined", 
            "abc", /(.)|(.)/g, function(m, m1, m2, i, s) { return m1+m2; });
