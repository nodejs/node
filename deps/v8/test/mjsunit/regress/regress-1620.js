// Copyright 2011 the V8 project authors. All rights reserved.
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

// Don't allow malformed unicode escape sequences in identifiers.
// In strings and regexps we currently allow malformed unicode escape
// sequences without throwing a SyntaxError. Instead "\u22gk" would
// treat the "\u" as an identity escape, and evaluate to "u22gk".
// Due to code sharing, we did the same in identifiers. This should
// no longer be the case.
// See: http://code.google.com/p/v8/issues/detail?id=1620

assertThrows("var \\u\\u\\u = 42;");
assertThrows("var \\u41 = 42;");
assertThrows("var \\u123 = 42;");
eval("var \\u1234 = 42;");
assertEquals(42, eval("\u1234"));
assertThrows("var uuu = 42; var x = \\u\\u\\u");

// Regressions introduced and fixed again while fixing the above.

// Handle 0xFFFD correctly (it's a valid value, and shouldn't be used
// to mark an error).
assertEquals(0xFFFD, "\uFFFD".charCodeAt(0));

// Handle unicode escapes in regexp flags correctly.
assertThrows("/x/g\\uim", SyntaxError);
assertThrows("/x/g\\u2im", SyntaxError);
assertThrows("/x/g\\u22im", SyntaxError);
assertThrows("/x/g\\u222im", SyntaxError);
assertThrows("/x/g\\\\u2222im", SyntaxError);
