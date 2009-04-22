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

// Test that we get exceptions for invalid left-hand sides.  Also
// tests that if the invalid left-hand side is a function call, the
// exception is delayed until runtime.

// Normal assignments:
assertThrows("12 = 12");
assertThrows("x++ = 12");
assertThrows("eval('var x') = 12");
assertDoesNotThrow("if (false) eval('var x') = 12");

// Pre- and post-fix operations:
assertThrows("12++");
assertThrows("12--");
assertThrows("--12");
assertThrows("++12");
assertThrows("++(eval('12'))");
assertThrows("(eval('12'))++");
assertDoesNotThrow("if (false) ++(eval('12'))");
assertDoesNotThrow("if (false) (eval('12'))++");

// For in:
assertThrows("for (12 in [1]) print(12);");
assertThrows("for (eval('var x') in [1]) print(12);");
assertDoesNotThrow("if (false) for (eval('var x') in [1]) print(12);");

// For:
assertThrows("for (12 = 1;;) print(12);");
assertThrows("for (eval('var x') = 1;;) print(12);");
assertDoesNotThrow("if (false) for (eval('var x') = 1;;) print(12);");

// Assignments to 'this'.
assertThrows("this = 42");
assertThrows("function f() { this = 12; }");
assertThrows("for (this in Array) ;");
assertThrows("for (this = 0;;) ;");
assertThrows("this++");
assertThrows("++this");
assertThrows("this--");
assertThrows("--this");


