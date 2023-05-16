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

// Test of constant folding of boolean-valued expressions.

// See http://code.google.com/p/v8/issues/detail?id=406

assertFalse(typeof(0) == "zero");
assertTrue(typeof(0) != "zero");

// The and and or truth tables with both operands constant.
assertFalse(typeof(0) == "zero" && typeof(0) == "zero");
assertFalse(typeof(0) == "zero" && typeof(0) != "zero");
assertFalse(typeof(0) != "zero" && typeof(0) == "zero");
assertTrue(typeof(0) != "zero" && typeof(0) != "zero");

assertFalse(typeof(0) == "zero" || typeof(0) == "zero");
assertTrue(typeof(0) == "zero" || typeof(0) != "zero");
assertTrue(typeof(0) != "zero" || typeof(0) == "zero");
assertTrue(typeof(0) != "zero" || typeof(0) != "zero");

// Same with just the left operand constant.
// Helper function to prevent simple constant folding.
function one() { return 1; }

assertFalse(typeof(0) == "zero" && one() < 0);
assertFalse(typeof(0) == "zero" && one() > 0);
assertFalse(typeof(0) != "zero" && one() < 0);
assertTrue(typeof(0) != "zero" && one() > 0);

assertFalse(typeof(0) == "zero" || one() < 0);
assertTrue(typeof(0) == "zero" || one() > 0);
assertTrue(typeof(0) != "zero" || one() < 0);
assertTrue(typeof(0) != "zero" || one() > 0);

// Same with just the right operand constant.
assertFalse(one() < 0 && typeof(0) == "zero");
assertFalse(one() < 0 && typeof(0) != "zero");
assertFalse(one() > 0 && typeof(0) == "zero");
assertTrue(one() > 0 && typeof(0) != "zero");

assertFalse(one() < 0 || typeof(0) == "zero");
assertTrue(one() < 0 || typeof(0) != "zero");
assertTrue(one() > 0 || typeof(0) == "zero");
assertTrue(one() > 0 || typeof(0) != "zero");
