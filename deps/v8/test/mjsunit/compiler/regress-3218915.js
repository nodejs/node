// Copyright 2010 the V8 project authors. All rights reserved.
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

// Regression test for failure to deoptimize properly when the most recent
// side effect occurred in a comma expression in an effect context.

// An unoptimizable function, calling it is a side effect.
function side_effect() { try {} finally {} return "wrong"; }

// A function to observe the value of its first argument.
function observe(x, y) { try {} finally {} return x; }

// If we optimize for x a smi, then x a string will deopt.  The side effect
// immediately before the deopt is in a comma expresion in an effect context
// (i.e., itself the left subexpression of a comma expression).
function test(x) { return observe(this, ((0, side_effect()), x + 1)); }

// Run test enough times to get it optimized.
for (var i = 0; i < 1000000; ++i) test(0);

// Force test to deopt.  If it behaves normally, it should return the global
// object.  If the value of the call to side_effect() is lingering after the
// deopt, it will return the string "wrong".
assertFalse(test("a") === "wrong");
