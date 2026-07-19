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

assertTrue(!x && typeof x == 'undefined');
assertTrue(!y && typeof y == 'undefined');
if (false) { var x = 42; }
if (true)  { var y = 87; }
assertTrue(!x && typeof x == 'undefined');
assertEquals(87, y);

assertTrue(!z && typeof z == 'undefined');
if (false) { var z; }
assertTrue(!z && typeof z == 'undefined');

assertThrows("var \u2E2F;", SyntaxError);
assertThrows("var \\u2E2F;", SyntaxError);

assertDoesNotThrow("var \u2118;");
assertDoesNotThrow("var \\u2118;");
assertDoesNotThrow("var \u212E;");
assertDoesNotThrow("var \\u212E;");
assertDoesNotThrow("var \u309B;");
assertDoesNotThrow("var \\u309B;");
assertDoesNotThrow("var \u309C;");
assertDoesNotThrow("var \\u309C;");

assertDoesNotThrow("var $\u00B7;");
assertDoesNotThrow("var $\u0387;");
assertDoesNotThrow("var $\u1369;");
assertDoesNotThrow("var $\u1370;");
assertDoesNotThrow("var $\u1371;");
assertDoesNotThrow("var $\u19DA;");
